#!/usr/bin/env python3

# Script to download and keep in sync routes from a Ride-With-GPS account.

# https://github.com/ridewithgps/developers
# https://docs.python.org/3/howto/urllib2.html
# https://mypy.readthedocs.io/en/stable/cheat_sheet_py3.html
import os, ssl, urllib.request, json, io, logging, sys, shutil, sqlite3, errno
from email.policy import EmailPolicy
from datetime import datetime
from typing import Any, Dict, Optional

logger = logging.getLogger('rwgps_sync_routes')

status_codes = {
    200 : '200 - OK',
    201 : '201 - Created',
    404 : '404 - Not Found',
    400 : '400 - Bad Request',
    403 : '403 - Forbiden',
    500 : '500 - Internal Server Error'
}

# Characters which are invalid in file names, at least on Windows.  These will
# be removed from the file name before using it...
invalid_filename_chars = str.maketrans('', '', '<>:"/\\|?*')

def status_to_string(status_code):
    try:
        return status_codes[status_code]
    except:
        return f"Unknown status code: {status_code}"

def maybe_create_db_schema(db: sqlite3.Connection):
    """Create the tables for the RWGPS state database, if they don't already
    exist.  We can open a SQLite3 file and always call this function to ensure
    the data tables we need are there.

    """

    # table to store settings as key/value pairs
    db.execute('''
    create table if not exists rwgps_settings(
        id integer not null primary key,
        key text unique not null,
        value text)
    ''')

    # table to store metadata for each route we download -- this is mainly
    # used to determine the local file name corresponding to a route ID -- so
    # we know what file to delete when the route is deleted on ridewithgps.com
    db.execute('''
    create table if not exists rwgps_route_metadata(
        id integer not null primary key, -- matches the RWGPS route id
        gpx_url text not null,
        gpx_file text not null,
        json_url text not null,
        json_file text not null)
    ''')

def db_get_setting(
        db: sqlite3.Connection,
        key: str,
        default: Optional[str] = None) -> Optional[str]:
    """Return the settig value for KEY from the database (rwgps_settings
    table), or return DEFAULT, if there is no value for KEY.

    """
    c = db.cursor()
    c.execute("select value from rwgps_settings where key = ?", (key,))
    r = c.fetchone()
    return r[0] if r is not None else default

def db_put_setting(
        db: sqlite3.Connection,
        key: str,
        value: Optional[str]) -> None:
    """Store or update the setting KEY + VALUE in the database

    """
    c = db.cursor()
    c.execute('''
        insert into rwgps_settings(key, value) values (?, ?)
            on conflict(key) do update set value = excluded.value
    ''', (key, value))
    db.commit()

def db_put_route_metadata(
        db: sqlite3.Connection,
        route_id: int,
        gpx_url: str,
        gpx_file: str,
        json_url: str,
        json_file: str) -> None:
    """Store metadata about a route in the database.  We store the URLs and
    file locations, both in GPX and original JSON formats, since we cannot
    always re-create file names, especially when we need to delete routes...

    """
    c = db.cursor()
    c.execute('''
        insert into rwgps_route_metadata(id, gpx_url, gpx_file, json_url, json_file)
        values(?, ?, ?, ?, ?)
        on conflict(id) do update set
            gpx_url = excluded.gpx_url,
            gpx_file = excluded.gpx_file,
            json_url = excluded.json_url,
            json_file = excluded.json_file''',
        (route_id, gpx_url, gpx_file, json_url, json_file))
    db.commit()

def db_get_route_metadata(
        db: sqlite3.Connection,
        route_id: int) -> Optional[tuple[str, str, str, str]]:
    """Return route metadata for the specified route ID.  Returns a tuple of 4
    values: URL for the GPX file, local file for the GPX file, and URL and
    local files for the JSON format of the route.

    Returns None if the route ID is not found in the database.

    """
    c = db.cursor()
    c.execute('''
        select gpx_url, gpx_file, json_url, json_file
        from rwgps_route_metadata
        where id = ?''', (route_id,))
    r = c.fetchone()
    return (r[1], r[2], r[3], r[4]) if r is not None else None

def db_get_all_route_metadata(
        db: sqlite3.Connection) -> list[tuple[str, str, str, str]]:
    c = db.cursor()
    c.execute('''
        select gpx_url, gpx_file, json_url, json_file
        from rwgps_route_metadata
    ''')
    return c.fetchall()

def db_del_route_metadata(
        db: sqlite3.Connection,
        route_id: int) -> None:
    """Delete metadata associated with the specified route ID.  Does nothing
    if there is no such route ID in the database.

    """
    c = db.cursor()
    c.execute("delete from rwgps_route_metadata where id = ?", (route_id,))
    db.commit()

def remove_file(file_name: str) -> None:
    """Remove the file in FILE_NAME, but don't raise an exception if the file
    does not exist.  Exceptions are still raised if the file cannot be removed
    for other reasons.

    """
    try:
        os.remove(file_name)
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise     # this is not a "file does not exist error", re-raise it

def maybe_create_parent_dir(file_name: str):
    """Create the parent directory for FILE_NAME, if it does not already exist.

    """
    (d, f) = os.path.split(file_name)
    if d != '' and d != '.':
        os.makedirs(d, exist_ok = True)

class RwgpsSettings():
    authToken: Optional[str]
    syncDirGpx: Optional[str]
    syncDirJson: Optional[str]
    nextSyncUrl: str

    def __init__(self, settings_file: str, logger: logging.Logger):
        self.apiBaseUrl = 'https://ridewithgps.com'
        self.sslContext = ssl.create_default_context()
        self.logger = logger

        self._read_settings_file(settings_file)
        self._read_state_file(self.stateFile)
        self._configure_logger(logger)

        if self.authToken is None:
            self.logger.info("Fetching authentication token from server...")
            self.authToken = self._fetch_token_from_server()

        if self.syncDirGpx is not None:
            os.makedirs(self.syncDirGpx, exist_ok = True)

        if self.syncDirJson is not None:
            os.makedirs(self.syncDirJson, exist_ok = True)

    def _read_settings_file(self, settings_file: str) -> None:
        """Read the settings file and setup this object.  This will give us
        the API key and user account credentials, among other things.

        An exception is raised if the settings file does not exist, or is not
        a valid JSON file.
        """
        with open(settings_file, 'r') as f:
            data = json.load(f)
            try:
                self.apiKey = data['apiKey']
            except KeyError:
                raise Exception(f"missing apiKey from {settings_file}")
            try:
                self.userEmail = data['userEmail']
            except KeyError:
                raise Exception(f"missing userEmail from {settings_file}")
            try:
                self.userPassword = data['userPassword']
            except KeyError:
                raise Exception(f"missing userPassword from {settings_file}")

            self.syncDirGpx = data.get('syncDirGpx', './sync-data/gpx')
            self.syncDirJson = data.get('syncDirJson', './sync-data/json')
            self.stateFile = data.get('stateFile', './rwgps-state.db')
            self.logFile = data.get('logFile', None)

    def _read_state_file(self, state_file: str) -> None:
        """Read sync state.  This will create the sync database if it does not
        exist, and will fetch the authentication token and next sync URL, if
        they exist.

        """
        maybe_create_parent_dir(state_file)
        self.db = sqlite3.connect(state_file)
        maybe_create_db_schema(self.db)
        c = self.db.cursor()
        self.authToken = db_get_setting(self.db, 'authToken', None)
        nextSyncUrl = db_get_setting(self.db, 'nextSyncUrl', None)
        if nextSyncUrl is None:
            self.logger.info(f"nextSyncUrl set to default, will fetch all data")
            self.nextSyncUrl = 'https://ridewithgps.com/api/v1/sync.json?since=1970-01-01&assets=routes'
        else:
            self.nextSyncUrl = nextSyncUrl

    def _configure_logger(self, logger: logging.Logger) -> None:
        """Configure the passed in logger.  The settings file can specify
        where log messages go to, so this will setup LOGGER to output to that
        file.

        """

        logger.setLevel(logging.INFO)
        ch = logging.StreamHandler(sys.stderr)
        ch.setLevel(logging.INFO)
        fmt = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
        ch.setFormatter(fmt)
        logger.addHandler(ch)
        if self.logFile is not None:
            maybe_create_parent_dir(self.logFile)
            fh = logging.FileHandler(self.logFile)
            fh.setLevel(logging.INFO)
            fmt = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            fh.setFormatter(fmt)
            logger.addHandler(fh)

    def write_state_file(self) -> None:
        """Write the current state (which has to be persisted) to the state
        database.

        """
        if self.db is None:
            raise Exception("write_state_file: database is not open")
        db_put_setting(self.db, 'authToken', self.authToken)
        db_put_setting(self.db, 'nextSyncUrl', self.nextSyncUrl)

    def make_get_request(self, url: str) -> urllib.request.Request:
        """Prepare and return a urllib request to GET the passed in URL.  This
        adds the Rwgps authentication token to the request.

        """
        if self.authToken is None:
            raise Exception("make_get_request: authToken is not set")
        return urllib.request.Request(
            url = url,
            method = 'GET',
            headers = {
                'x-rwgps-api-key' : self.apiKey,
                'x-rwgps-auth-token' : self.authToken
            })

    def _fetch_token_from_server(self) -> Optional[str]:
        """Get a new authentication token from the Rwgps server based on the
        user credentials.

        """
        body = {
            'user': {
                'email' : self.userEmail,
                'password': self.userPassword
            }}
        request = urllib.request.Request(
            url = f"{self.apiBaseUrl}/api/v1/auth_tokens.json",
            method = 'POST',
            headers = {
                'Content-Type' : 'application/json',
                'x-rwgps-api-key' : self.apiKey
            },
            data = bytearray(json.dumps(body), 'utf-8'))
        with urllib.request.urlopen(request, context = self.sslContext) as u:
            if (u.status != 201):
                raise Exception(status_to_string(u.status))
            else:
                try:
                    result = json.load(u)
                    return result['auth_token']['auth_token']
                except json.decoder.JSONDecodeError:
                    raise Exception(f"_fetch_token_form_server: bad response: {result}")

    def fetch_json(self, url: str) -> Dict[str, Any]:
        """Fetch JSON data at URL and return the JSON object.  Uses Rwgps
        authenticatin for the URL.

        """
        request = self.make_get_request(url)
        with urllib.request.urlopen(request, context = self.sslContext) as u:
            if (u.status != 200):
                raise Exception(status_to_string(u.status))
            else:
                return json.load(u)

    def fetch_into_file(
            self,
            url: str,
            folder: str,
            file_name: Optional[str],
            expected_content_type: Optional[str] = None) -> str:
        """Fetch the URL using Rwgps authentication and store the output in
        the FOLDER+FILE.

        If FILE_NAME is None, obtain the filename from the server response.

        If EXPECTED_CONTENT_TYPE is not None, check that the returned
        Content-Type matches the expected one.

        """
        request = self.make_get_request(url)
        with urllib.request.urlopen(request, context = self.sslContext) as u:
            # if an expected_content_type was specified, check that the
            # returned Content-Type is what we expected
            if expected_content_type is not None:
                ct = EmailPolicy.header_factory('content-type', u.getheader('Content-Type'))
                if ct.content_type != expected_content_type:
                    raise Exception(f"Bad content type: {ct.content_type}, expecting {expected_content_type}")
            # if no file was specified as an argument, look for the
            # content-disposition header if it specifies a file name in
            # there...
            if file_name is None:
                h = u.getheader('Content-Disposition')
                cd = EmailPolicy.header_factory('content-disposition', h)
                p = dict(cd.params)
                # The curious conversion below ensures that the file name is
                # encoded as UTF8, since the header_factory above decodes
                # strings as latin1 (ascii).  We also remove any characters
                # which are invalid in file names to ensure the filename is
                # actually usable...
                file_name = p['filename'] \
                    .encode('latin1') \
                    .decode('utf-8') \
                    .translate(invalid_filename_chars)
            ofile = f"{folder}/{file_name}"
            with open(ofile, "wb") as f:
                shutil.copyfileobj(u, f)
            return ofile

def rwgps_process_created_items(settings, created_items):
    for item in created_items:
        try:
            jurl = item['item_url']
            iid = item['item_id']
            jfile = f"{settings.syncDirJson}/{iid}.json"
            logger.info(f"Downloading {jurl}")
            # we could use fetch_into_file(), which would be more efficient,
            # but json.dump() gives us nicely indented data
            data = settings.fetch_json(jurl)
            with open(jfile, "w") as f:
                json.dump(data, f, indent = 4)
            gurl = f"https://ridewithgps.com/routes/{iid}.gpx"
            logger.info(f"Downloading {gurl}")
            gfile = settings.fetch_into_file(gurl, f"{settings.syncDirGpx}/", None, "application/gpx+xml")
            db_put_route_metadata(settings.db, iid, gurl, gfile, jurl, jfile)
        except Exception as e:
            logger.warning(f"While creating items, ignoring exception {e} for entry {item}")

def rwgps_process_deleted_items(settings, deleted_items):
    for item in deleted_items:
        try:
            iid = item['item_id']
            m = db_get_route_metadata(settings.db, iid)
            if m is None:
                continue
            (gpx_url, gpx_file, json_url, json_file) = m
            logger.info(f"Removing {gpx_file}")
            remove_file(gpx_file)
            logger.info(f"Removing {json_file}")
            remove_file(json_file)
            db_del_route_metadata(settings.db, iid)
        except Exception as e:
            logger.warning(f"While deleting items, ignoring exception {e} for entry {item}")

def rwgps_resync_missig_items(settings: RwgpsSettings):
    """Check if any items preset in our metadata database have been removed
    form disk and re-download them.

    """
    for (gpx_url, gpx_file, json_url, json_file) in \
            db_get_all_route_metadata(settings.db):
        if not os.path.exists(gpx_file):
            logger.warning(f"Re-fetching, {gpx_url}")
            (d, f) = os.path.split(gpx_file)
            settings.fetch_into_file(gpx_url, d, f, "application/gpx+xml")
        if not os.path.exists(json_file):
            logger.warning(f"Re-fetching, {json_url}")
            data = settings.fetch_json(json_url)
            with open(json_file, "w") as j:
                json.dump(data, j, indent = 4)

def rwgps_sync_routes(settings: RwgpsSettings):
    logger.info("Fetching synchronization list...")
    sync_data = settings.fetch_json(settings.nextSyncUrl)
    logger.info(f"Received {len(sync_data['items'])} items to process...")
    created_items = {}
    deleted_items = {}
    for item in sync_data['items']:
        try:
            # we asked for routes, but who knows what we get back, so better
            # check
            if item['item_type'] != 'route':
                continue
            action = item['action']
            iid = item['item_id']
            if action == 'created' or action == 'updated':
                created_items[iid] = item
            elif action == 'deleted':
                deleted_items[iid] = item
                try:
                    del created_items[iid]
                except KeyError:
                    pass
            elif action == 'added' or action == 'removed':

                # These are add / remove from collection, which we don't
                # handle, but we could -- for example if the user pins a route
                # form another user, we should probably download that route...

                collection_name = None
                try:
                    collection_name = item['collection']['name']
                except KeyError:
                    pass
                to_or_from = 'to' if action == 'added' else 'from'
                logger.info(f"Item {iid} {action} {to_or_from} collection {collection_name} (won't process this event)")
                pass
            else:
                logger.info(f"Skipping unknown action {action}")
        except Exception as e:
            logger.warning(f"While processing items, ignoring exception {e} for entry {item}")

    rwgps_process_deleted_items(settings, deleted_items.values())
    rwgps_process_created_items(settings, created_items.values())
    settings.nextSyncUrl = sync_data['meta']['next_sync_url']

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(
        prog='rwgps-sync',
        description='Synchronize routes from rwgps.com')
    parser.add_argument('settings_file')
    args = parser.parse_args()
    settings = RwgpsSettings(args.settings_file, logger)
    rwgps_sync_routes(settings)
    rwgps_resync_missig_items(settings)
    settings.write_state_file()
