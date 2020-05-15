# Sample configuration files

Here you can find several configuration file corresponding to difference situations.

## evqueue-mini.conf

For the impatients! About the smallest possible configuration file. Just configure your SQL server access and your done.

Default parameters will be user for all other paremters. Engine will only listen on localhost and default ports.

/tmp folder is used for must of the paths.

This should be enough for testing purposes.

## evqueue-small.conf

Same as above but including network configuration. This is mandatory if you want other users to access your server.

## evqueue-cluster-1.conf and evqueue-cluster-2.conf

A sample cluster configuration for two nodes running on the same machine. Ports have been changed to allow both engines to start.

Also note that **core.ipc.qid** must be changed if multiple engines run on the same machine. This is not necessary if they run on different machines. This is the ID of the message queue used for children to communicate with their parent daemon.
