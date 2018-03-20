# dht
Distributed Hash Table

Implementation of a distributed hash table

Building on Linux:
------------------
This program has two external dependencies:
* ASIO
* Google Protobuf 3.5.1

You can download Google Protobuf from:
[Google Protobuf 3.5.1](https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-cpp-3.5.1.tar.gz)

Unzip it and follow the instructions in protobuf-3.5.1/src/README.md to build and install.

ASIO can be cloned from the public source repository at:
[chriskohlhoff/asio](https://github.com/chriskohlhoff/asio)

Please checkout version 1-10-branch of the ASIO library using the following command:
cd ..
git clone https://github.com/chriskohlhoff/asio.git -b asio-1-10-branch

Resulting directory structure:
* asio\asio\include
* dht\README.md

Building on Windows:
--------------------
I used VisualStudio 2017 Community Edition version 15.6.1 (Pre-release)
I used vcpkg to install protobuf based on the instructions here:
[Microsoft/vcpkg](https://github.com/Microsoft/vcpkg)
[google/protobuf/src/README.md](https://github.com/google/protobuf/blob/master/src/README.md)
based on a recommendation I found here:
[ras0219-msft/protobuf](https://github.com/ras0219-msft/protobuf/commit/10a5095df0ba387bc283859d4126b06771930274)

Open solution file msvc\dht.sln
Select your preferred target.  (Mine is Debug/x64)

Select **Build | Build Solution** from the menu


Notes and Limitations:
----------------------

* Distribution algorithm causes more key movement than I would like when adding new nodes
* Call management could be simplified quite a bit through re-factoring
* Nodes do not shut down, and there is no support for disconnected Nodes
* Occasionally, I have seen clients not shutdown gracefully - Needs to be fixed

**Warning:** Joining a Node that is bound to the loopback adapter will cause other Nodes in the DHT not to be able to communicate with it

