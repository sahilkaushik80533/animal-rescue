# Automated Stray Animal Rescue Dispatch System

## Overview
This project is an emergency triage system designed to help rescue critically injured stray animals faster. Instead of handling calls in the order they are received (first-come, first-served), this system automatically calculates urgency and prioritizes life-threatening cases.

The core logic is powered by a high-speed C engine built from scratch, which is connected to a modern web interface using a Python bridge.

## Features
* **Dynamic Prioritization:** Automatically sorts rescue calls based on urgency using a custom C logic engine.
* **No Memory Limits:** Uses dynamic memory allocation (`malloc` and `free`) with a Singly Linked List so the system never crashes from too many calls.
* **Instant History Log:** Uses a Last-In, First-Out (LIFO) Stack to save completed dispatches instantly.
* **Fast Searching:** Uses a Binary Search Tree (BST) to find specific locations and animal types without slowing down.
* **Web Interface:** A simple, high-contrast dashboard built with plain HTML, CSS, and JavaScript.

## Technology Stack
* **Core Engine:** C (Custom Data Structures and Algorithms)
* **Backend:** Python (Flask API)
* **Frontend:** HTML, CSS, Vanilla JavaScript
* **Database:** SQLite 
* **Deployment:** Live on Render

## How It Works (Architecture)
This project uses a hybrid architecture, similar to how modern machine learning systems are built:
1. The **Python Flask** server handles the website clicks and stores basic data.
2. Instead of doing the heavy lifting itself, Python securely passes the data to the **compiled C program**.
3. The **C program** performs all the complex sorting, filtering, and memory management at maximum speed.
4. The result is sent back to the website for the dispatcher to see.

## How to Run Locally
To run this project on your own computer, follow these steps:

   **Install Requirements:** 
   Open your terminal and install the required Python packages:
   ```bash
   pip install -r requirements.txt

   gcc engine.c -o engine

   python app.py
   (Or run python start.py if you are using the setup script).

   Open your web browser and go to http://localhost:5000 to use the system.
   
