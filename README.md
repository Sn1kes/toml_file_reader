# Multithreaded file copier app
This application was a test task.
Requirements:
* App must be a Qt console application
* App must parse .toml config for parameters, such as:
  * Array of file paths to read from (input)
  * Array of file paths to write to (output)
  * File reading speed
* App must read file from input array and write it to the corresponding file in output array
* Each file must be processed in different thread
* Reading buffer size needs to be chosen dynamically, with accordance to specified reading speed
* App needs to be cross-platdorm by using C++ standard library facilities
