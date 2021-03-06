# Concurrent library

## Requirements

[GTest v1.10.0](https://github.com/google/googletest/tree/v1.10.x) - "googletest is a testing framework developed by the
Testing Technology team with Google's specific requirements and constraints in mind".

## Options

- `CONCURRENT_TESTING` - enable build tests
- `CONCURRENT_EXAMPLES`- enable build examples

## Build library

1 Clone project:

```cmd
git clone https://git.shs.tools/zzuummaa/concurrent_lib.git
```

2 Generate cmake files:

```cmd
cd concurrent
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

3 Build and install library (use shell with administrative privileges or sudo):

```cmd
cmake --build . --target install
```

4 Use in your project cmake files:

```cmake
find_package(Concurrent REQUIRED)
add_executable(your_app ${YOUR_SOURCES})
target_link_libraries(your_app concurrent)
```

## Build tests

1 Clone GTest:

```cmd
git clone --branch v1.10.x https://github.com/google/googletest.git
```

2 Generate cmake files:

```cmd
cd googletest
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```
3 Build and install library (use shell with administrative privileges or sudo):

```cmd
cmake --build . --target install
```

4 Check cmake output. GTest and GMock libs should be installed;

5 Remove `FindGTest.cmake` from `<cmake_dir>/modules`.