<pre>
                                                      _.-````'-,_
                                            _,.,_ ,-'`           `'-.,_
                                          /)     (\                   '``-.
                                         ((      ) )                      `\
Y88b   d88P    d88888888888b. 888888b.    \)    (_/                        )\
 Y88b d88P    d88888888  "Y88b888  "88b    |       /)           '    ,'    / \
  Y88o88P    d88P888888    888888  .88P    `\    ^'            '     (    /  ))
   Y888P    d88P 888888    8888888888K.      |      _/\ ,     /    ,,`\   (  "`
    888    d88P  888888    888888  "Y88b      \Y,   |  \  \  | ````| / \_ \
    888   d88P   888888    888888    888        `)_/    \  \  )    ( >  ( >
    888  d8888888888888  .d88P888   d88P                 \( \(     |/   |/
    888 d88P     8888888888P" 8888888P"                 /_(/_(    /_(  /_(
</pre>

YADB is a SQL database written in modern C++, built from scratch as an
educational project. YADB is focused on being simple, functional, and correct. As a result,
performance and scalability are intentionally *not* the top priorities—clarity
and educational value come first.

> **⚠️ DISCLAIMER**: YADB is in **very early stages** of development. Many features are incomplete,
> unstable, or missing entirely. This is an educational project and should not be used for
> production purposes. Expect bugs, breaking changes, and incomplete functionality.

---

## Contents

- [Blog Series](#blog-series)
- [Building](#building)
- [Usage](#usage)
- [Roadmap](#roadmap)

---

## Blog Series

While I'm building YADB, I figured that it would be a good exercise to document
my progress and share some of the design decisions I made along the way.
You can check out the blog [here](https://www.erichayter.com/yadb-docs/).

| Date | Post |
|------|------|
| 14/03/2026 | [dev/3: liftoff](https://www.erichayter.com/yadb-docs/dev/2026/03/14/liftoff.html) |
| 11/09/2025 | [dev/2: no tests, no failures](https://www.erichayter.com/yadb-docs/dev/2025/09/11/no-tests-no-failures.html) |
| 15/06/2025 | [dev/1: Page Buffer Manager and Asynchronous Disk I/O in YADB](https://www.erichayter.com/yadb-docs/dev/2025/06/15/page-buffer-manager-and-async-disk-io.html) |
| 02/06/2025 | [Database Internals: Page Buffers](https://www.erichayter.com/yadb-docs/meta/2025/06/02/page-buffer.html) |
| 29/05/2025 | [dev/0: Hello, World!](https://www.erichayter.com/yadb-docs/dev/2025/05/29/hello-world.html) |

---

## Building

1. Clone the repository and submodules:
```bash
git clone --recursive https://github.com/EricHayter/yadb
```

2. Build using CMake:
```bash
cd yadb
mkdir build
cd build
cmake ..
cmake --build .
```

---

## Usage

After building, you can run the YADB shell:

```bash
# From the build/ directory
./src/shell/yadb-shell
```

### Demo

Here's a quick demo of what YADB can currently do:

```sql
-- Create a table
CREATE TABLE users (name TEXT, age INTEGER);

-- Insert some data
INSERT INTO users VALUES (Alice, 30);
INSERT INTO users VALUES (Bob, 25);
INSERT INTO users VALUES (Charlie, 35);

-- Query the data
SELECT * FROM users;
```

**Output:**
```
┌─────────┬─────┐
│ name    │ age │
├─────────┼─────┤
│ Alice   │ 30  │
│ Bob     │ 25  │
│ Charlie │ 35  │
└─────────┴─────┘
3 rows in set
```

You can also select specific columns:

```sql
SELECT name FROM users;
```

### Running Tests

Tests can be built and run for individual components:

```bash
# From the build/ directory
cmake --build . --target test
```

---

## Roadmap

- [x] Set up blog
- [x] Implement disk manager
- [x] Implement disk scheduler
- [x] Implement page buffer
- [x] Implement slotted page interface
- [x] Implement lexer and parser for SQL subset
- [x] Implement shell interface
- [x] Implement basic executor with type-safe operations
- [x] Implement result printer with formatted table output
- [ ] Implement optimizer
  - [x] Basic iterator-based query execution (Volcano model)
  - [x] FileScanIterator and ProjectionIterator
  - [ ] SelectionIterator (WHERE clause filtering)
  - [ ] JoinIterator
  - [ ] Implement external sorting
  - [ ] Cost-based optimization
- [ ] Implement B+ tree storage engine
- [ ] Add transaction support
- [ ] Add concurrency control

