CREATE TABLE IF NOT EXISTS event (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT NOT NULL,
    type INTEGER NOT NULL,
    file TEXT NOT NULL,
    line INTEGER NOT NULL,
    message TEXT NOT NULL,
    backtrace TEXT NOT NULL,
    uri TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS slow_request (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT NOT NULL,
    duration FLOAT NOT NULL,
    file TEXT NOT NULL);
