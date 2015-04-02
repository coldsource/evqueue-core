# evqueue

evQueue is an open source job scheduler and queueing engine. It features an event-driven C++ engine and a PHP / MySQL web control interface which provides tasks monitoring and creation. See the PHP frontend [here](https://github.com/coldsource/evqueue-frontend-php).

It provides both simple task execution and complex task chaining (workflow) using XML and XPath description. Workflow description includes output linking, conditions and loops. Queues management provides an easy way for task parallelization and resource control.

The network API provides an easy way, XML based, for synchronous or asynchronous workflow launching and control, allowing tasks to be launched from external applications or web pages.

evQueue is agentless (remote connection is made through SSH) and works on Linux environments. We provide debian packages. For other distributions you can easily compile from source.

For documentation and binary download, [visit the official website!](http://www.evqueue.net/)

## Building

Once you met the dependancy requirements (see below), you can build it using
CMake:

``` 
mkdir build
cd build
cmake ../
make
```

### Debian

These packages are required to build from the source:

- build-essential
- cmake
- libmysqlclient-dev
- libxqilla-dev
- libxerces-c-dev
