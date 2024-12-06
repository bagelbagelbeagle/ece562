import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder, StandardScaler
from sklearn.metrics import accuracy_score, roc_auc_score, f1_score, precision_score, recall_score, roc_curve, auc
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeClassifier
from sklearn.neighbors import KNeighborsClassifier
from imblearn.over_sampling import SMOTE
import seaborn as sns
import matplotlib.pyplot as plt
from math import pi

# Load dataset
file_path = '/Users/muntasirmamun/Downloads/TEST.csv'
data = pd.read_csv(file_path)

# Encode 'Access Type' as numeric values
data['Access Type'] = LabelEncoder().fit_transform(data['Access Type'])

# Separate features and target variable
X = data.drop(columns=['Hit/Miss'])  # Features
y = data['Hit/Miss']  # Target

# Plot Correlation Matrix
plt.figure(figsize=(12, 10))
corr_matrix = X.corr()
sns.heatmap(corr_matrix, annot=True, fmt=".2f", cmap="coolwarm", square=True)
plt.title("Feature Correlation Matrix")
plt.show()

# Plot Cache Hit vs. Miss Distribution
data['Hit/Miss'].value_counts().plot(kind='bar', color=['skyblue', 'salmon'])
plt.title("Cache Hit vs. Miss Distribution")
plt.xlabel("Hit/Miss")
plt.ylabel("Count")
plt.show()

# Split into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.4, random_state=42)

# Apply SMOTE to balance the training data
smote = SMOTE(random_state=42)
X_train_balanced, y_train_balanced = smote.fit_resample(X_train, y_train)

# Standardize the features
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train_balanced)
X_test_scaled = scaler.transform(X_test)

# Dictionary to store models and their names
models = {
    "Logistic Regression": LogisticRegression(),
    "Decision Tree": DecisionTreeClassifier(),
    "K-Nearest Neighbors": KNeighborsClassifier(n_neighbors=5)
}

# Train and evaluate each model
for model_name, model in models.items():
    print(f"Evaluating {model_name}")

    # Train model
    model.fit(X_train_scaled, y_train_balanced)

    # Make predictions
    y_pred = model.predict(X_test_scaled)
    y_prob = model.predict_proba(X_test_scaled)[:, 1] if hasattr(model, "predict_proba") else y_pred

    # Calculate metrics
    accuracy = accuracy_score(y_test, y_pred)
    auc_score = roc_auc_score(y_test, y_prob)
    f1 = f1_score(y_test, y_pred)
    precision = precision_score(y_test, y_pred)
    recall = recall_score(y_test, y_pred)

    print(f"  Accuracy: {accuracy:.4f}")
    print(f"  AUC Score: {auc_score:.4f}")
    print(f"  F1 Score: {f1:.4f}")
    print(f"  Precision: {precision:.4f}")
    print(f"  Recall: {recall:.4f}")

    # ROC Curve
    fpr, tpr, _ = roc_curve(y_test, y_prob)
    roc_auc = auc(fpr, tpr)

    # Plot ROC curve
    plt.figure()
    plt.plot(fpr, tpr, color='blue', label=f'ROC curve (AUC = {roc_auc:.2f})')
    plt.plot([0, 1], [0, 1], color='gray', linestyle='--')
    plt.xlabel('False Positive Rate')
    plt.ylabel('True Positive Rate')
    plt.title(f'Receiver Operating Characteristic (ROC) Curve - {model_name}')
    plt.legend(loc="lower right")
    plt.show()

# Radar Chart for Hit vs. Miss Mean Values

# Calculate mean values for hits and misses for numeric features only
hit_mean = data[data['Hit/Miss'] == 1].mean(numeric_only=True)
miss_mean = data[data['Hit/Miss'] == 0].mean(numeric_only=True)
features = hit_mean.index

# Prepare data for radar chart
values_hit = hit_mean[features].values
values_miss = miss_mean[features].values
angles = [n / float(len(features)) * 2 * pi for n in range(len(features))]
angles += angles[:1]  # Complete the loop

# Plot radar chart
plt.figure(figsize=(8, 8))
ax = plt.subplot(111, polar=True)
plt.xticks(angles[:-1], features, color='grey', size=8)
ax.plot(angles, np.append(values_hit, values_hit[0]), linewidth=1, linestyle='solid', label="Hit")
ax.plot(angles, np.append(values_miss, values_miss[0]), linewidth=1, linestyle='solid', label="Miss")
ax.fill(angles, np.append(values_hit, values_hit[0]), 'b', alpha=0.1)
ax.fill(angles, np.append(values_miss, values_miss[0]), 'r', alpha=0.1)
plt.legend(loc='upper right')
plt.title('Radar Chart of Cache Performance Features for Hits vs. Misses')
plt.show()
