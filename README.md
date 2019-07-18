# QuickFOIL

## Library Dependencies
QuickFOIL uses two classes (fbvector and StringPiece) provided by the open-source C++ library Folly. This dependency is not critical and will be removed in the future. The Folly code is included in the third_party directory and thus not required to be pre-installed on your system.

The following required libraries are not included in the third_party directory.
- BOOST version 1.49.0 or higher. The required components are system, filesystem, regex and program_options. The components regex and program_options are required by Folly.
- LibEvent. It is a dependent library for Folly. The dependency will be removed after Folly becomes an optional dependency.

## Getting Started
### Build
``` 
# Create a build directory
mkdir build
cd build
# Generate build files.
# If you want to use a debug build, use -DCMAKE_BUILD_TYPE=Debug instead.
cmake -DCMAKE_BUILD_TYPE=Release ..
# Build
make quickfoil
```
The ```make quickfoil``` will create an executable "quickfoil" under the build directory.

CMake options
- -DBOOST_ROOT=\<Boost root path>: The root path to BOOST. Required if CMake fails to find Boost libraries in the standard paths.
- -DLIBEVENT_ROOT=\<LibEvent root path>: The root path to LibEvent. Required if CMake fails to find LibEvent libraries in the standard paths.
- -DENABLE_LOGGING=1: Enable basic logging for intermediate runtime results in the Release build. The logging is always enabled in the Debug build, no matter whether this is enabled.
- -DENABLE_TIMING=1: Measure execution time for each phase. The total execution time is always measured even when this option is turned off.
- -DMONITOR_MEMORY=1: Keep track of the memory usage. QuickFOIL may use the info to avoid selecting candidate literals that could blow up memory usage.

For example, to build in the release mode with a specified boost root path and the logging enabled, type
```
cmake -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT=/opt/local/lib/boost -DENABLE_LOGGING=1 ..
```

### Run
The only required command-line option to run quickfoil is the path to a JSON file, which gives the input schema and the data file paths. 

JSON file format.
```
{
  // The target predicate/relation name.
  "target" : "bongard",
  // All the background predicates that can be used to define the target predicate.
  "background" : [ 
    "element",
    "circle"
  ],
  // Optional test data set.
  "test" : {
    // Test file.
    // In all data files, each tuple is on one line with '|' as the column delimiter.
    "file" : "../data/bongard/th3/100/fold_0/bongard_test",
    // Number of positive test tuples.
    "num_positive" : 10
  },
  // Array that defines each of the background predicates and the target predicate.
  "relations" : [
    {
      // Target predicate.
      "name" : "bongard",
      // Required for the target predicate.
      "num_positive" : 88,
      "file" : "../data/bongard/th3/100/fold_0/bongard_train_5",
      "attributes" : [
        {
          // The only attribute/argument for bondard is of type picture.
          // (Integer 0 is used to represent the type).
          "domain_type" : 0
        }
      ]
    },
    {
      // Background predicate element(picture, object)
      "name" : "element",
      "file" : "../data/bongard/th3/100/fold_0/element",
      "attributes" : [
        {
          // The first attribute/argument for element is of type picture
          // (encoded in 0).
          "domain_type" : 0
        },
        {
          // The second attribute/argument for element is of type object
          // (encoded in 1).
          "domain_type" : 1
        }
      ]
    },
... 
```

Example data sets can be found in sample_data/.

For configuration options, type
```
./quickfoil --help
```

Possibly useful options include
```
-maximum_clause_length: The maximum number of body literals in a clause.
-positive_threshold: The minimum percentage of positive traning examples need to be covered by output clauses.
```

Choose smaller values for the following three options if QuickFOIL fails to find any rule or takes too long time to find a useful rule.
```
-minimum_true_precision: The minimum precision of an output clause on the training examples (default: 0.8).
-minimum_f_score: The minimum F-value of an output clause on the training examples (default: 0.85).
-minimum_inflated_precision: The minimum precision on the binding set (the ratio of the positive bindings to the total bindings) for a candidate clause to be considered for output (default: 0.85). This affects the time to stop a rule search iteration.
```
