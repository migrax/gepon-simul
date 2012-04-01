/*
 * gepon-simul: A simulator for upstream sleep mode for ONUs in GEPON Networks.
 * (C) 2011—2013 Miguel Rodríguez Pérez <miguel@det.uvigo.es>
 */

/* 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <iostream>
#include <libgen.h>
#include <cstdlib>
#include <cmath>
#include <cassert>

namespace {
  void usage(const char *basename) {
    std::cerr << "Uso: " << basename << " [-p psize (bytes)] [-q qw (packets)] [-u nominal uplink (Mb/s)] [-a avg uplink (Mb/s)] [-d dba cycle (ms)] [-w wake up (ms)] [-r refresh (ms)] [-s simulation length (s)]" << std::endl;
  }
  
  double get_next_arrival(void) {
    double arrival_time;
    
    std::cin >> arrival_time;
    
    return arrival_time;
  }
  
  class EPONSimulator {
  public:
    enum status {OFF, TRANSITION_TO_ON, ON, TRANSITION_TO_OFF};
    
  private:
    // Parameters
    const size_t psize;
    const unsigned long int uplink_capacity, uplink_avg, qw;
    const double dbac_len, wakeup_len, refresh_to;
    const double max_off;
    
    // State
    enum status curr_state;
    double curr_time, next_arrival;
    double next_refresh_due;
    /*long int*/ size_t curr_qsize;
    size_t pending_drain; // In low upstream reserved bw with big packets we do not transmit a a constat upstream rate.

    size_t get_psize(void) const {
      return psize;
    }

    int insert_into_queue(size_t size) {
      curr_qsize += size;

      std::cout << "I " << size << " " << curr_qsize << " @ " << curr_time << std::endl;

      return curr_qsize;
    }

    int drain_from_queue(size_t bits) {
      bits += pending_drain;

      std::cout << "D " << bits << " " << curr_qsize << " @ " << curr_time << std::endl;
      while (bits >= get_psize() && curr_qsize >= get_psize()) {
	size_t tx_traffic = get_psize();

	curr_time += tx_traffic/static_cast<double>(uplink_capacity);
	curr_qsize -= tx_traffic;
	bits -= tx_traffic;

	std::cout << "L " << tx_traffic << " " << curr_qsize << " @ " << curr_time << std::endl;
      }

      pending_drain = bits;

      return curr_qsize;
    }

    enum status change_state(enum status new_state) {
      const char *status_names[] = {
	"OFF",
	"TON",
	"ON",
	"TOFF"
      };
      std::cout << "C " << status_names[curr_state] << " → " << status_names[new_state]
		<< " @ " << curr_time << std::endl;
      return curr_state = new_state;
    }

    double get_next_dbac_start(void) const {
      static const double error = 1e-9; // To prevent curr_time == start of DBA. Rounding errors can make next_dba = current_dba
      int completed_cycles = floor((curr_time + error)/dbac_len);

      return (completed_cycles + 1)*dbac_len;
    }

    double get_transmit_time(void) const {
      double needed_tx_time = uplink_avg / uplink_capacity;
      double dba_cycle_util = dbac_len - needed_tx_time;
      double start_tx_offset = dba_cycle_util * (random() / static_cast<double>(RAND_MAX));
      double dbac_start = get_next_dbac_start() - dbac_len;

      assert(dbac_start + start_tx_offset > curr_time); /* Must be called at the start of the cycle */

      return dbac_start + start_tx_offset;
    }

    // Returns the start of the next DBA - wakeup_time
    double get_ton_time(void) const {
      double next_dba = get_next_dbac_start();
      double tentative_start = next_dba - wakeup_len;
      if (curr_time > tentative_start)
	tentative_start += dbac_len;

      return tentative_start;
    }

    void proc_arrival_off(void) {
      if (next_refresh_due < 0)
	next_refresh_due = curr_time + max_off;

      if (next_arrival < 0)
	next_arrival = get_next_arrival();

      curr_time = next_arrival;
      insert_into_queue(get_psize());

      next_arrival = -1; // Next time we want to get a new packet

      if (curr_qsize >= qw || curr_time >= next_refresh_due - dbac_len) {
	const double transition_time = get_ton_time();

	do {
	  next_arrival = get_next_arrival();
	  if (next_arrival <= transition_time)
	    insert_into_queue(get_psize());
	} while(next_arrival <= transition_time);
	curr_time = transition_time;
	next_refresh_due = -1;
	change_state(TRANSITION_TO_ON);
      }
    }

    void proc_arrival_ton() {
      /* We need first DBA to send R and receiveG,
	 and we transmit in the second */
      const double exit_time = curr_time + wakeup_len + 1*dbac_len;

      while (next_arrival <= exit_time) { // We have a next_arrival from off
	insert_into_queue(get_psize());
	next_arrival = get_next_arrival();
      }
      curr_time = exit_time;
      change_state(ON);
    }

    void proc_arrival_on() {
      //const double tx_time = uplink_avg/(double)uplink_capacity;
      const double tx_bits = uplink_avg*dbac_len;
      const double tx_time = tx_bits/static_cast<double>(uplink_capacity);

      /* Hay que averiguar si la cola se vacía en este DBA.
       * Primero decidimos en que instante del ciclo transmitimos y durante cuanto tiempo
       * Los paquetes pueden llegar antes de tx, después (en el mismo ciclo) o durante (lo asimilamos a antes)
       */
      while (curr_qsize > 0) {
	if (next_arrival < 0)
	  next_arrival = get_next_arrival();

	curr_time = get_transmit_time();
	const double end_send_time = curr_time + tx_time;

	while (end_send_time > next_arrival) {
	  insert_into_queue(get_psize());
	  next_arrival = get_next_arrival();
	}
	//curr_time = end_send_time;
	//curr_qsize -= tx_bits;
	drain_from_queue(tx_bits);

	assert(curr_qsize >= 0);

	if (curr_qsize == 0) {
	  // If curr_qsize < 0 correct the end of tx time
	  //curr_time += curr_qsize/(double)uplink_capacity;
	  //curr_qsize = 0;
	  change_state(TRANSITION_TO_OFF);
	} else {
	  curr_time = get_next_dbac_start();
	  while (next_arrival < curr_time) {
	    if (next_arrival < 0)
	      next_arrival = get_next_arrival();

	    insert_into_queue(get_psize());
	    next_arrival = get_next_arrival();
	  }
	}
      }
    }

    void proc_arrival_toff(void) {
      // TOFF will end in the first half of the next dbac (we assue than the report is sent there)
      const double next_dbac = get_next_dbac_start();
      const double end_time = next_dbac + (random()/static_cast<double>(RAND_MAX))*dbac_len/2.;
      assert(next_arrival >= 0);

      while(next_arrival < end_time) {
	      curr_time = next_arrival;
	      insert_into_queue(get_psize());
	      next_arrival = get_next_arrival();
      }

      curr_time = end_time;

      if (curr_qsize < qw)	
	      change_state(OFF);
      else {
	      curr_time = next_dbac; // HACK: On expects to start at start of a DBA cycle
	      change_state(ON);
      }
    }

  public:
    EPONSimulator(int psize = 1500, long int uplink_capacity = 10000000000L, long int uplink_avg = 50000000L, int qw = 100,
		  double dbac_len = 2e-3, double wakeup_len = 1e-3, double refresh_to = 50e-3) : psize(8*psize), uplink_capacity(uplink_capacity),
												 uplink_avg(uplink_avg), qw(8*psize*qw), dbac_len(dbac_len), wakeup_len(wakeup_len), refresh_to(refresh_to), max_off(50e-3), curr_state(OFF),
												 curr_time(0.), next_arrival(-1.), next_refresh_due(-1), curr_qsize(0), pending_drain(0) {};

    void run(double end_time = 100.) {
      while (curr_time < end_time) {
	switch (curr_state) {
	case OFF:
	  proc_arrival_off(); break;
	case TRANSITION_TO_ON:
	  proc_arrival_ton(); break;
	case TRANSITION_TO_OFF:
	  proc_arrival_toff(); break;
	case ON:
	  proc_arrival_on(); break;
	}
      }
    }
  };
}

using namespace std;

int main(int argc, char *argv[]) {
  double simul_len = 100;
  int psize = 1500;
  long int uplink_capacity = 10000000000L;
  long int uplink_avg = 50000000L;
  int qw = 10;
  double dbac_len = 2e-3;
  double wakeup_len = 1e-3;
  double refresh_to = 50e-3;

  int opt;

  srandom(1);

  while ((opt = getopt(argc, argv, "p:q:u:a:d:w:r:s:")) != -1) {
    long int *lgp;
    double *dp;
    switch(opt) {
    case 'p': psize = atoi(optarg); break;
    case 'q': qw = atoi(optarg); break;
    case 'u': lgp = &uplink_capacity;
    case 'a': lgp  = &uplink_avg;
      *lgp  = 1000000 * atoi(optarg);
      break;
    case 'd': dp = &dbac_len;
    case 'w': dp = &wakeup_len;
    case 'r': dp = &refresh_to;
      *dp =  atof(optarg)/1.e3;
      break;
    case 's': simul_len = atof(optarg); break;
    default:
      usage(basename(argv[0]));
      return 1;
    }
  }

  EPONSimulator simul(psize, uplink_capacity, uplink_avg, qw, dbac_len, wakeup_len, refresh_to);

  simul.run(simul_len);

  return 0;
}
