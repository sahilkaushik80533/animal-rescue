#!/usr/bin/env bash
# Render runs this during the BUILD phase (before the app starts).
# It compiles the C engine and initializes the SQLite database.

set -e

echo "--- Installing Python dependencies ---"
pip install -r requirements.txt

echo "--- Compiling C DSA engine ---"
gcc engine.c -o engine -Wall
chmod +x engine
echo "engine compiled OK"

echo "--- Initializing SQLite database ---"
python -c "
import sqlite3
conn = sqlite3.connect('rescues.db')
f = open('schema.sql', 'r')
conn.executescript(f.read())
conn.commit()
conn.close()
print('rescues.db ready')
"

echo "--- Build complete ---"
