import torch
import torch.nn as nn
import torchvision
import torchvision.transforms as transforms
import torch_pruning as tp
import optuna
import copy
from lenet5 import LeNet5, num_classes, batch_size, device

# ============= DATA =================
train_dataset = torchvision.datasets.MNIST(
    root='./data', train=True,
    transform=transforms.Compose([
        transforms.Resize((32, 32)),
        transforms.ToTensor(),
        transforms.Normalize(mean=(0.1307,), std=(0.3081,))
    ]),
    download=True
)

test_dataset = torchvision.datasets.MNIST(
    root='./data', train=False,
    transform=transforms.Compose([
        transforms.Resize((32, 32)),
        transforms.ToTensor(),
        transforms.Normalize(mean=(0.1325,), std=(0.3105,))
    ]),
    download=True
)

train_loader = torch.utils.data.DataLoader(dataset=train_dataset, batch_size=batch_size, shuffle=True)
test_loader = torch.utils.data.DataLoader(dataset=test_dataset, batch_size=batch_size, shuffle=True)

# ============= LOAD BASELINE =================
baseline_model = LeNet5(num_classes).to(device)
baseline_model.load_state_dict(torch.load('model/baseline.pth', map_location=device))

# Count baseline params for reference
example_inputs = torch.randn(1, 1, 32, 32).to(device)
baseline_macs, baseline_params = tp.utils.count_ops_and_params(baseline_model, example_inputs)
print(f"Baseline: MACs={baseline_macs/1e6:.2f}M, Params={baseline_params/1e3:.1f}K")


def evaluate(model):
    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for images, labels in test_loader:
            images, labels = images.to(device), labels.to(device)
            outputs = model(images)
            _, predicted = torch.max(outputs.data, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()
    return 100.0 * correct / total


def fine_tune(model, epochs, lr):
    model.train()
    optimizer = torch.optim.Adam(model.parameters(), lr=lr)
    cost = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        for data, target in train_loader:
            data, target = data.to(device), target.to(device)
            optimizer.zero_grad()
            output = model(data)
            loss = cost(output, target)
            loss.backward()
            optimizer.step()


def prune_and_evaluate(model, ratio_fc, ratio_fc1, iterative_steps, fine_tune_epochs, fine_tune_lr):
    """Run structured pruning + fine-tuning, return (accuracy, param_count)."""
    imp = tp.importance.GroupMagnitudeImportance(p=2)
    ignored_layers = [model.layer1, model.layer2, model.relu, model.relu1, model.fc2]
    pruning_ratio_dict = {model.fc: ratio_fc, model.fc1: ratio_fc1}

    pruner = tp.pruner.BasePruner(
        model=model,
        example_inputs=example_inputs,
        importance=imp,
        pruning_ratio_dict=pruning_ratio_dict,
        ignored_layers=ignored_layers,
        iterative_steps=iterative_steps,
    )

    for i in range(iterative_steps):
        for group in pruner.step(interactive=True):
            group.prune()

        # Re-init optimizer after architecture change, then fine-tune
        fine_tune(model, fine_tune_epochs, fine_tune_lr)

    accuracy = evaluate(model)
    _, nparams = tp.utils.count_ops_and_params(model, example_inputs)
    return accuracy, nparams


# ============= OPTUNA OPTIMIZATION =================
def objective(trial):
    # Sample hyperparameters
    ratio_fc = trial.suggest_float("ratio_fc", 0.3, 0.9, step=0.1)
    ratio_fc1 = trial.suggest_float("ratio_fc1", 0.2, 0.7, step=0.1)
    iterative_steps = trial.suggest_int("iterative_steps", 3, 6)
    fine_tune_epochs = trial.suggest_int("fine_tune_epochs", 3, 8)
    fine_tune_lr = trial.suggest_float("fine_tune_lr", 1e-4, 1e-2, log=True)

    # Deep copy baseline so each trial starts fresh
    model = copy.deepcopy(baseline_model).to(device)

    accuracy, nparams = prune_and_evaluate(
        model, ratio_fc, ratio_fc1, iterative_steps, fine_tune_epochs, fine_tune_lr
    )

    # Store for later analysis
    trial.set_user_attr("accuracy", accuracy)
    trial.set_user_attr("nparams", nparams)

    print(f"Trial {trial.number}: ratio_fc={ratio_fc}, ratio_fc1={ratio_fc1}, "
          f"steps={iterative_steps}, ft_epochs={fine_tune_epochs}, ft_lr={fine_tune_lr:.5f} "
          f"=> Acc={accuracy:.2f}%, Params={nparams/1e3:.1f}K")

    return accuracy, nparams


if __name__ == "__main__":
    # Multi-objective: maximize accuracy, minimize params
    study = optuna.create_study(
        directions=["maximize", "minimize"],
        study_name="lenet5_pruning",
    )

    study.optimize(objective, n_trials=20)

    # ============= RESULTS =================
    print("\n===== PARETO-FRONT =====")
    for trial in study.best_trials:
        print(f"Trial {trial.number}: "
              f"Acc={trial.values[0]:.2f}%, "
              f"Params={trial.values[1]/1e3:.1f}K, "
              f"Config={trial.params}")

    # Save the best model (highest accuracy among pareto-optimal)
    best_trial = max(study.best_trials, key=lambda t: t.values[0])
    print(f"\nBest trial: {best_trial.number} with Acc={best_trial.values[0]:.2f}%, Params={best_trial.values[1]/1e3:.1f}K")

    # Re-run best config to get the actual model for saving
    best_model = copy.deepcopy(baseline_model).to(device)
    acc, params = prune_and_evaluate(
        best_model,
        best_trial.params["ratio_fc"],
        best_trial.params["ratio_fc1"],
        best_trial.params["iterative_steps"],
        best_trial.params["fine_tune_epochs"],
        best_trial.params["fine_tune_lr"],
    )
    torch.save(best_model, "model/pruned_best.pth")
    print(f"Saved best model: Acc={acc:.2f}%, Params={params/1e3:.1f}K")
