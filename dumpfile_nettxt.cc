/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include <errno.h>

#include "globalregistry.h"
#include "gpsdclient.h"
#include "dumpfile_nettxt.h"

Dumpfile_Nettxt::Dumpfile_Nettxt() {
	fprintf(stderr, "FATAL OOPS: Dumpfile_Nettxt called with no globalreg\n");
	exit(1);
}

Dumpfile_Nettxt::Dumpfile_Nettxt(GlobalRegistry *in_globalreg) : 
	Dumpfile(in_globalreg) {
	globalreg = in_globalreg;

	txtfile = NULL;

	if (globalreg->netracker == NULL) {
		fprintf(stderr, "FATAL OOPS:  Netracker missing before Dumpfile_Nettxt\n");
		exit(1);
	}

	if (globalreg->kismet_config == NULL) {
		fprintf(stderr, "FATAL OOPS:  Config file missing before Dumpfile_Nettxt\n");
		exit(1);
	}

	// Find the file name
	if ((fname = ProcessConfigOpt("nettxt")) == "" || globalreg->fatal_condition) {
		return;
	}

	if ((txtfile = fopen(fname.c_str(), "w")) == NULL) {
		_MSG("Failed to open nettxt log file '" + fname + "': " + strerror(errno),
			 MSGFLAG_FATAL);
		globalreg->fatal_condition = 1;
		return;
	}

	_MSG("Opened nettxt log file '" + fname + "'", MSGFLAG_INFO);

}

Dumpfile_Nettxt::~Dumpfile_Nettxt() {
	// Close files
	if (txtfile != NULL) {
		Flush();
		fclose(txtfile);
		_MSG("Closed nettxt log file '" + fname + "'", MSGFLAG_INFO);
	}

	txtfile = NULL;

	if (export_filter != NULL)
		delete export_filter;
}

int Dumpfile_Nettxt::Flush() {
	if (txtfile == NULL)
		return 0;

	rewind(txtfile);

	fprintf(txtfile, "Kismet (http://www.kismetwireless.net)\n"
			"%.24s - Kismet %s.%s.%s\n"
			"-----------------\n\n",
			ctime(&(globalreg->start_time)),
			VERSION_MAJOR, VERSION_MINOR, VERSION_TINY);

	// Get the tracked network and client->ap maps
	const map<mac_addr, Netracker::tracked_network *> tracknet =
		globalreg->netracker->FetchTrackedNets();
	const multimap<mac_addr, Netracker::tracked_client *> trackcli =
		globalreg->netracker->FetchAssocClients();

	map<mac_addr, Netracker::tracked_network *>::const_iterator x;
	multimap<mac_addr, Netracker::tracked_client *>::const_iterator y;

	int netnum = 0;

	// Dump all the networks
	for (x = tracknet.begin(); x != tracknet.end(); ++x) {
		netnum++;

		if (export_filter->RunFilter(x->second->bssid, mac_addr(0), mac_addr(0)))
			continue;

		Netracker::tracked_network *net = x->second;

		if (net->type == network_remove)
			continue;

		string ntype;
		switch (net->type) {
			case network_ap:
				ntype = "infrastructure";
				break;
			case network_adhoc:
				ntype = "ad-hoc";
				break;
			case network_probe:
				ntype = "probe";
				break;
			case network_data:
				ntype = "data";
				break;
			case network_turbocell:
				ntype = "turbocell";
				break;
			default:
				ntype = "unknown";
				break;
		}

		fprintf(txtfile, "Network %d: BSSID %s\n", netnum, 
				net->bssid.Mac2String().c_str());
		fprintf(txtfile, " First      : %.24s\n", ctime(&(net->first_time)));
		fprintf(txtfile, " Last       : %.24s\n", ctime(&(net->last_time)));
		fprintf(txtfile, " Type       : %s\n", ntype.c_str());
		fprintf(txtfile, " BSSID      : %s\n", net->bssid.Mac2String().c_str());

		if (net->ssid.length() > 0) {
			for (map<uint32_t, string>::iterator m = net->beacon_ssid_map.begin();
				 m != net->beacon_ssid_map.end(); ++m) {
				if (m->second.length() > 0) {
					fprintf(txtfile, " SSID       : \"%s\"\n", m->second.c_str());
				}
			}
			fprintf(txtfile, " Last SSID  : \"%s\"\n", net->ssid.c_str());
		} else {
			if (net->ssid_cloaked)
				fprintf(txtfile, " SSID       : <Cloaked>\n");
			else
				fprintf(txtfile, " SSID       : <Unknown>\n");
		}

		if (net->beacon_info.length() > 0) {
			fprintf(txtfile, " BeaconInfo : \"%s\"\n", net->beacon_info.c_str());
		}
		fprintf(txtfile, " Channel    : %d\n", net->channel);
		fprintf(txtfile, " Max Rate   : %2.1f\n", net->maxrate);
		fprintf(txtfile, " Max Seen   : %ld\n", net->snrdata.maxseenrate * 100);

		if (net->snrdata.carrierset & (1 << (int) carrier_80211b))
			fprintf(txtfile, " Carrier    : IEEE 802.11b\n");
		if (net->snrdata.carrierset & (1 << (int) carrier_80211bplus))
			fprintf(txtfile, " Carrier    : IEEE 802.11b+\n");
		if (net->snrdata.carrierset & (1 << (int) carrier_80211a))
			fprintf(txtfile, " Carrier    : IEEE 802.11a\n");
		if (net->snrdata.carrierset & (1 << (int) carrier_80211g))
			fprintf(txtfile, " Carrier    : IEEE 802.11g\n");
		if (net->snrdata.carrierset & (1 << (int) carrier_80211fhss))
			fprintf(txtfile, " Carrier    : IEEE 802.11 FHSS\n");
		if (net->snrdata.carrierset & (1 << (int) carrier_80211dsss))
			fprintf(txtfile, " Carrier    : IEEE 802.11 DSSS\n");

		if (net->snrdata.encodingset & (1 << (int) encoding_cck))
			fprintf(txtfile, " Encoding   : CCK\n");
		if (net->snrdata.encodingset & (1 << (int) encoding_pbcc))
			fprintf(txtfile, " Encoding   : PBCC\n");
		if (net->snrdata.encodingset & (1 << (int) encoding_ofdm))
			fprintf(txtfile, " Encoding   : OFDM\n");

		if (net->cryptset == 0)
			fprintf(txtfile, " Encryption : None\n");
		if (net->cryptset & crypt_wep)
			fprintf(txtfile, " Encryption : WEP\n");
		if (net->cryptset & crypt_layer3)
			fprintf(txtfile, " Encryption : Layer3\n");
		if (net->cryptset & crypt_wep40)
			fprintf(txtfile, " Encryption : WEP40\n");
		if (net->cryptset & crypt_wep104)
			fprintf(txtfile, " Encryption : WEP104\n");
		if (net->cryptset & crypt_tkip)
			fprintf(txtfile, " Encryption : TKIP\n");
		if (net->cryptset & crypt_wpa)
			fprintf(txtfile, " Encryption : WPA\n");
		if (net->cryptset & crypt_psk)
			fprintf(txtfile, " Encryption : PSK\n");
		if (net->cryptset & crypt_aes_ocb)
			fprintf(txtfile, " Encryption : AES-OCB\n");
		if (net->cryptset & crypt_aes_ccm)
			fprintf(txtfile, " Encryption : AES-CCM\n");
		if (net->cryptset & crypt_leap)
			fprintf(txtfile, " Encryption : LEAP\n");
		if (net->cryptset & crypt_ttls)
			fprintf(txtfile, " Encryption : TTLS\n");
		if (net->cryptset & crypt_tls)
			fprintf(txtfile, " Encryption : TLS\n");
		if (net->cryptset & crypt_peap)
			fprintf(txtfile, " Encryption : PEAP\n");
		if (net->cryptset & crypt_isakmp)
			fprintf(txtfile, " Encryption : ISAKMP\n");
		if (net->cryptset & crypt_pptp)
			fprintf(txtfile, " Encryption : PPTP\n");

		fprintf(txtfile, " LLC        : %d\n", net->llc_packets);
		fprintf(txtfile, " Data       : %d\n", net->data_packets);
		fprintf(txtfile, " Crypt      : %d\n", net->crypt_packets);
		fprintf(txtfile, " Fragments  : %d\n", net->fragments);
		fprintf(txtfile, " Retries    : %d\n", net->retries);
		fprintf(txtfile, " Total      : %d\n", net->llc_packets + net->data_packets);
		fprintf(txtfile, " Datasize   : %lu\n", net->datasize);

		if (net->gpsdata.gps_valid) {
			fprintf(txtfile, " Min Pos    : Lat %f Lon %f Alt %f Spd %f\n", 
					net->gpsdata.min_lat, net->gpsdata.min_lon,
					net->gpsdata.min_alt, net->gpsdata.min_spd);
			fprintf(txtfile, " Max Pos    : Lat %f Lon %f Alt %f Spd %f\n", 
					net->gpsdata.max_lat, net->gpsdata.max_lon,
					net->gpsdata.max_alt, net->gpsdata.max_spd);
			fprintf(txtfile, " Peak Pos   : Lat %f Lon %f Alt %f\n", 
					net->snrdata.peak_lat, net->snrdata.peak_lon,
					net->snrdata.peak_alt);
			fprintf(txtfile, " Agg Pos    : AggLat %ld AggLon %ld AggAlt %ld "
					"AggPts %lu\n",
					net->gpsdata.aggregate_lat, net->gpsdata.aggregate_lon,
					net->gpsdata.aggregate_alt, net->gpsdata.aggregate_points);
		}

		if (net->guess_ipdata.ip_type > ipdata_factoryguess && 
			net->guess_ipdata.ip_type < ipdata_group) {
			string iptype;
			switch (net->guess_ipdata.ip_type) {
				case ipdata_udptcp:
					iptype = "UDP/TCP";
					break;
				case ipdata_arp:
					iptype = "ARP";
					break;
				case ipdata_dhcp:
					iptype = "DHCP";
					break;
				default:
					iptype = "Unknown";
					break;
			}

			fprintf(txtfile, " IP Type    : %s\n", iptype.c_str());
			fprintf(txtfile, " IP Block   : %s\n", 
					inet_ntoa(net->guess_ipdata.ip_addr_block));
			fprintf(txtfile, " IP Netmask : %s\n", 
					inet_ntoa(net->guess_ipdata.ip_netmask));
			fprintf(txtfile, " IP Gateway : %s\n", 
					inet_ntoa(net->guess_ipdata.ip_gateway));
		}

		fprintf(txtfile, " BSS Time   : %lu\n", net->bss_timestamp);
		fprintf(txtfile, " CDP Device : \"%s\"\n", net->cdp_dev_id.c_str());
		fprintf(txtfile, " CDP Port   : \"%s\"\n", net->cdp_port_id.c_str());

		int clinum = 0;

		// Get the client range pairs and print them out
		pair<multimap<mac_addr, Netracker::tracked_client *>::const_iterator, 
			multimap<mac_addr, Netracker::tracked_client *>::const_iterator> apclis = 
			trackcli.equal_range(net->bssid);
		for (y = apclis.first; y != apclis.second; ++y) {
			Netracker::tracked_client *cli = y->second;

			clinum++;

			if (cli->type == client_remove)
				continue;

			string ctype;
			switch (cli->type) {
				case client_fromds:
					ctype = "fromds";
					break;
				case client_tods:
					ctype = "tods";
					break;
				case client_interds:
					ctype = "interds";
					break;
				case client_established:
					ctype = "established";
					break;
				case client_adhoc:
					ctype = "ad-hoc";
					break;
				default:
					ctype = "unknown";
					break;
			}

			fprintf(txtfile, " Client %d: MAC %s\n", clinum, 
					cli->mac.Mac2String().c_str());
			fprintf(txtfile, "  First      : %.24s\n", ctime(&(cli->first_time)));
			fprintf(txtfile, "  Last       : %.24s\n", ctime(&(cli->last_time)));
			fprintf(txtfile, "  Type       : %s\n", ctype.c_str());
			fprintf(txtfile, "  BSSID      : %s\n", cli->bssid.Mac2String().c_str());

			fprintf(txtfile, "  Channel    : %d\n", cli->channel);
			fprintf(txtfile, "  Max Seen   : %ld\n", cli->snrdata.maxseenrate * 100);

			if (cli->snrdata.carrierset & (1 << (int) carrier_80211b))
				fprintf(txtfile, "  Carrier    : IEEE 802.11b\n");
			if (cli->snrdata.carrierset & (1 << (int) carrier_80211bplus))
				fprintf(txtfile, "  Carrier    : IEEE 802.11b+\n");
			if (cli->snrdata.carrierset & (1 << (int) carrier_80211a))
				fprintf(txtfile, "  Carrier    : IEEE 802.11a\n");
			if (cli->snrdata.carrierset & (1 << (int) carrier_80211g))
				fprintf(txtfile, "  Carrier    : IEEE 802.11g\n");
			if (cli->snrdata.carrierset & (1 << (int) carrier_80211fhss))
				fprintf(txtfile, "  Carrier    : IEEE 802.11 FHSS\n");
			if (cli->snrdata.carrierset & (1 << (int) carrier_80211dsss))
				fprintf(txtfile, "  Carrier    : IEEE 802.11 DSSS\n");

			if (cli->snrdata.encodingset & (1 << (int) encoding_cck))
				fprintf(txtfile, "  Encoding   : CCK\n");
			if (cli->snrdata.encodingset & (1 << (int) encoding_pbcc))
				fprintf(txtfile, "  Encoding   : PBCC\n");
			if (cli->snrdata.encodingset & (1 << (int) encoding_ofdm))
				fprintf(txtfile, "  Encoding   : OFDM\n");

			if (cli->cryptset == 0)
				fprintf(txtfile, "  Encryption : None\n");
			if (cli->cryptset & crypt_wep)
				fprintf(txtfile, "  Encryption : WEP\n");
			if (cli->cryptset & crypt_layer3)
				fprintf(txtfile, "  Encryption : Layer3\n");
			if (cli->cryptset & crypt_wep40)
				fprintf(txtfile, "  Encryption : WEP40\n");
			if (cli->cryptset & crypt_wep104)
				fprintf(txtfile, "  Encryption : WEP104\n");
			if (cli->cryptset & crypt_tkip)
				fprintf(txtfile, "  Encryption : TKIP\n");
			if (cli->cryptset & crypt_wpa)
				fprintf(txtfile, "  Encryption : WPA\n");
			if (cli->cryptset & crypt_psk)
				fprintf(txtfile, "  Encryption : PSK\n");
			if (cli->cryptset & crypt_aes_ocb)
				fprintf(txtfile, "  Encryption : AES-OCB\n");
			if (cli->cryptset & crypt_aes_ccm)
				fprintf(txtfile, "  Encryption : AES-CCM\n");
			if (cli->cryptset & crypt_leap)
				fprintf(txtfile, "  Encryption : LEAP\n");
			if (cli->cryptset & crypt_ttls)
				fprintf(txtfile, "  Encryption : TTLS\n");
			if (cli->cryptset & crypt_tls)
				fprintf(txtfile, "  Encryption : TLS\n");
			if (cli->cryptset & crypt_peap)
				fprintf(txtfile, "  Encryption : PEAP\n");
			if (cli->cryptset & crypt_isakmp)
				fprintf(txtfile, "  Encryption : ISAKMP\n");
			if (cli->cryptset & crypt_pptp)
				fprintf(txtfile, "  Encryption : PPTP\n");

			fprintf(txtfile, "  LLC        : %d\n", cli->llc_packets);
			fprintf(txtfile, "  Data       : %d\n", cli->data_packets);
			fprintf(txtfile, "  Crypt      : %d\n", cli->crypt_packets);
			fprintf(txtfile, "  Fragments  : %d\n", cli->fragments);
			fprintf(txtfile, "  Retries    : %d\n", cli->retries);
			fprintf(txtfile, "  Total      : %d\n", 
					cli->llc_packets + cli->data_packets);
			fprintf(txtfile, "  Datasize   : %lu\n", cli->datasize);

			if (cli->gpsdata.gps_valid) {
				fprintf(txtfile, "  Min Pos    : Lat %f Lon %f Alt %f Spd %f\n", 
						cli->gpsdata.min_lat, cli->gpsdata.min_lon,
						cli->gpsdata.min_alt, cli->gpsdata.min_spd);
				fprintf(txtfile, "  Max Pos    : Lat %f Lon %f Alt %f Spd %f\n", 
						cli->gpsdata.max_lat, cli->gpsdata.max_lon,
						cli->gpsdata.max_alt, cli->gpsdata.max_spd);
				fprintf(txtfile, "  Peak Pos   : Lat %f Lon %f Alt %f\n", 
						cli->snrdata.peak_lat, cli->snrdata.peak_lon,
						cli->snrdata.peak_alt);
				fprintf(txtfile, "  Agg Pos    : AggLat %ld AggLon %ld AggAlt %ld "
						"AggPts %lu\n",
						cli->gpsdata.aggregate_lat, cli->gpsdata.aggregate_lon,
						cli->gpsdata.aggregate_alt, cli->gpsdata.aggregate_points);
			}

			if (cli->guess_ipdata.ip_type > ipdata_factoryguess && 
				cli->guess_ipdata.ip_type < ipdata_group) {
				string iptype;
				switch (cli->guess_ipdata.ip_type) {
					case ipdata_udptcp:
						iptype = "UDP/TCP";
						break;
					case ipdata_arp:
						iptype = "ARP";
						break;
					case ipdata_dhcp:
						iptype = "DHCP";
						break;
					default:
						iptype = "Unknown";
						break;
				}

				fprintf(txtfile, "  IP Type    : %s\n", iptype.c_str());
				fprintf(txtfile, "  IP Block   : %s\n", 
						inet_ntoa(cli->guess_ipdata.ip_addr_block));
				fprintf(txtfile, "  IP Netmask : %s\n", 
						inet_ntoa(cli->guess_ipdata.ip_netmask));
				fprintf(txtfile, "  IP Gateway : %s\n", 
						inet_ntoa(cli->guess_ipdata.ip_gateway));
			}

			fprintf(txtfile, "  CDP Device : \"%s\"\n", cli->cdp_dev_id.c_str());
			fprintf(txtfile, "  CDP Port   : \"%s\"\n", cli->cdp_port_id.c_str());

		}

	}

	fflush(txtfile);

	return 1;
}


