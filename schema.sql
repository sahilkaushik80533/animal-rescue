-- ================================================================
--  STRAY ANIMAL RESCUE DISPATCH SYSTEM — Database Schema
-- ================================================================
--  This creates the SQLite table that stores all rescue calls.
--  Status is either 'pending' or 'dispatched'.
-- ================================================================

CREATE TABLE IF NOT EXISTS rescues (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    location      TEXT    NOT NULL,
    animal_type   TEXT    NOT NULL,
    description   TEXT    NOT NULL,
    urgency       INTEGER NOT NULL CHECK(urgency BETWEEN 1 AND 10),
    status        TEXT    NOT NULL DEFAULT 'pending',
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    dispatched_at TIMESTAMP
);
