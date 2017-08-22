#!/usr/bin/env python3
#
# Fetch the EPO file (GPS satelite data, "Extended Prediction Orbit") from the
# Garmin site.  based off
# https://www.kluenter.de/garmin-ephemeris-files-and-linux/

import urllib.request, sys, os, argparse, syslog;

def fetch_epo_data():
    req = urllib.request.Request(
        url='http://omt.garmin.com/Rce/ProtobufApi/EphemerisService/GetEphemerisData',
        data=b'\x0a-\x0a\x07express\x12\x05de_DE\x1a\x07Windows\"\x12601 Service Pack 1\x12\x0a\x08\x8c\xb4\x93\xb8\x0e\x12\x00\x18\x00\x18\x1c\x22\x00',
        headers={"Garmin-Client-Name" : "CoreService",
                 "Content-Type" : "application/octet-stream"});
    f = urllib.request.urlopen(req);

    # the data is received as a series of chunks with a 3 byte header followed by
    # 2304 bytes of payload (representing satelite data for 6 hours).  We need to
    # discard the 3 bytes, as the GARMIN device does not use them.  Not sure why,
    # see the referenced web sites for more details.

    chunk_size = 2304;
    epo_data = bytearray();

    while not f.closed:
        f.read(3);                      # discard 3 bytes
        chunk = f.read(chunk_size);
        if len(chunk) == 0:
            break;
        if len(chunk) != chunk_size:
            raise Exception('invalid chunk size');
        epo_data.extend(chunk);

    return epo_data;

if __name__ == '__main__':
    opt_parser = argparse.ArgumentParser(
        description = "Fetch Extended Prediction Orbit (EPO) data from Garmin.",
	epilog = "HINT: file should be placed in GARMIN/REMOTESW/EPO.BIN");
    opt_parser.add_argument(
        "-s", "--syslog",
        help="use syslog for messages",
        action="store_true");
    opt_parser.add_argument(
        "output_file",
        help="file to write EPO data");
    args = opt_parser.parse_args();

    if args.syslog:
        syslog.openlog("fetch-epo", 0, syslog.LOG_USER);

    try:
        if args.syslog:
            syslog.syslog(syslog.LOG_INFO, "started up, fetching EPO data");
        else:
            sys.stdout.write("Fetching epo data...\n");
        epo_data = fetch_epo_data();
        if not args.syslog:
            sys.stdout.write("Fetching epo data...done\n");
        with open(args.output_file, 'wb') as o:
            o.write(epo_data);
        if args.syslog:
            syslog.syslog(syslog.LOG_INFO, "wrote EPO data to %s" % (args.output_file,));
        else:
            sys.stdout.write("Wrote data to %s\n" % (args.output_file,));
    except Exception as e:
        if args.syslog:
            syslog.syslog(syslog.LOG_ERROR, e.str());
        else:
            sys.stdout.write(e.str());

    if args.syslog:
        syslog.closelog();

### Local Variables:
### mode: python
### mode: goto-address
### End:
