import copy
import torch
import torch.nn as nn
import torchvision
import torchvision.transforms as transforms
import torch_pruning as tp
from lenet5 import LeNet5, num_classes, batch_size, device

# Trial 7 params (Pareto favorite: Acc 99.34%, 18.2K params)
RATIO_FC = 0.7
RATIO_FC1 = 0.7
ITERATIVE_STEPS = 4
FINE_TUNE_EPOCHS = 4
FINE_TUNE_LR = 0.000232

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

example_inputs = torch.randn(1, 1, 32, 32).to(device)


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
        fine_tune(model, fine_tune_epochs, fine_tune_lr)

    accuracy = evaluate(model)
    _, nparams = tp.utils.count_ops_and_params(model, example_inputs)
    return accuracy, nparams


# ============= RUN =================
baseline_model = LeNet5(num_classes).to(device)
baseline_model.load_state_dict(torch.load('model/baseline.pth', map_location=device))

model = copy.deepcopy(baseline_model).to(device)
acc, nparams = prune_and_evaluate(model, RATIO_FC, RATIO_FC1, ITERATIVE_STEPS, FINE_TUNE_EPOCHS, FINE_TUNE_LR)

torch.save(model, "model/pruned_best.pth")
print(f"Saved model/pruned_best.pth: Acc={acc:.2f}%, Params={nparams/1e3:.1f}K")
