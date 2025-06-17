# Yet Another Database (yadb)

**yadb** is a SQL database written in modern C++, built from scratch as an
educational project.

This project serves two main goals:
1. To gain a deeper understanding of database internals.
2. To practice designing and implementing clean, modular systems in C++.

The focus of yadb is on being **simple, functional, and correct**. As a result,
performance and scalability are intentionally *not* the top priorities—clarity
and educational value come first.

Check out the [yadb blog](https://www.erichayter.com/yadb-docs/) for
development updates and posts about the underlying design and database
concepts.

## Contents

- [Building](#building)
- [Usage](#usage)
- [Roadmap](#roadmap)

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

## Usage

yadb currently doesn't include a standalone executable to run queries against.
However, tests can be built and run for the components implemented so far.

```
# From the build/ directory
cmake --build . --target yadb_test
./yadb_test
```

## Roadmap

- [x] Set up blog
- [x] Implement disk manager
- [x] Implement disk scheduler
- [x] Implement page buffer
- [x] Implement page guards
- [ ] Implement B+ trees
- [ ] TBD

