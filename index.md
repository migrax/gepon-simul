gepon-simul
===========

A simulator for upstream sleep mode for ONUs in GEPON Networks.

Overview
--------

This is the simulator for a sleep aware ONU as described in the paper [¨Improving Energy Efficiency in Upstream EPON Channels by Packet Coalescing.¨ (IEEE Trans. on Communications. 2012;60:929-32.).](http://dx.doi.org/10.1109/TCOMM.2012.022712.110142A)
Usage
-----

The simulator expects to be fed its standard input the packet arrivals tiems of the traffic to be transmitted to the OLT.
Invocation

`simulator [-p psize (bytes)] [-q qw (packets)] [-u nominal uplink (Mb/s)] [-a avg uplink (Mb/s)] [-d dba cycle (ms)] [-w wake up (ms)] [-r refresh (ms)] [-s simulation length (s)]`

Output
------

The simulator outputs a line every time an important event happens:

    `I size queue_size @ time`: Every time a packet arrives
    `L size queue_size @ time`: Every time a packet is removed from the upstream queue
    `C old_state new_state @ time`: When there is a status change

Legal
-----

Copyright ⓒ Miguel Rodríguez Pérez <miguel@det.uvigo.es> 2011—2013

This simulator is licenses under the GNU General Public License, version 3 (GPL-3.0). For for information see LICENSE.txt

