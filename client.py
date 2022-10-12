from enum import Enum
from threading import Thread, active_count
from select import select
import socket

CRLF = '\r\n'
B_CRLF = b'\r\n'


class MODE(Enum):
    PASV = 1
    PORT = 2
    NONE = 999


class SENDTYPE(Enum):
    LIST = 1
    RETR = 2
    NONE = 999


mode = MODE.NONE
sock = None
size = 8192
data = None
conn = None

ip = None
port = None
addr = None
about_to_send = SENDTYPE.NONE
send_name = None

user = None
islogin = False
pwd = None


def connect_ftp(ip, port):
    global sock
    if sock:
        close_socket()
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        errno = sock.connect_ex((ip, port))
        if errno == 0:
            res, num = get_control_msg()
            print_control_msg(res)
            if res[-1][:3] == '420':
                close_socket()
        else:
            print("Error:", errno)
            close_socket()
    except Exception as e:
        print(str(e))
        if sock:
            close_socket()


def close_socket(word='QUIT'):
    global sock
    global user
    global pwd
    global islogin
    global ip
    global port
    global about_to_send
    global send_name

    ip = None
    port = None
    user = None
    pwd = None
    islogin = False
    about_to_send = SENDTYPE.NONE
    send_name = None

    if sock:
        try:
            sock.send((word + CRLF).encode())
            res, num = get_control_msg()
            print_control_msg(res)
            sock.close()
        except Exception as e:
            print(str(e))
            if isinstance(e, socket.timeout):
                sock.close()
        sock = None
    close_data()


def get_control_msg(interrupt=False):
    global sock
    if sock:
        try:
            num = 0
            msg = sock.recv(size).decode()
            res = msg.strip().split(CRLF)
            if interrupt:
                while res[-1][3] != ' ':
                    msg = sock.recv(size).decode()
                    res.extend(msg.strip().split(CRLF))
            else:
                while res[-1][3] != ' ' or res[-1][0] == '1':
                    msg = sock.recv(size).decode()
                    res.extend(msg.strip().split(CRLF))
            for i in res:
                if i[3] == ' ':
                    num += 1
            return res, num
        except Exception as e:
            print(str(e))
            raise e


def print_control_msg(res):
    for i in res:
        print(i)


def async_print_control_msg():
    res, num = get_control_msg()
    print_control_msg(res)


def open_port(msg):
    global data
    close_data()
    try:
        data = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        data.settimeout(1)
        msg = msg.split(',')
        ip = '.'.join(msg[:4])
        port = int(msg[4]) * 256 + int(msg[5])
        data.bind((ip, port))
        data.listen(1)
    except Exception as e:
        if data:
            data.close()
            data = None
        raise e


def open_pasv(msg: str):
    global conn
    global data
    close_data()
    try:
        conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        begin = 0
        end = len(msg)
        for i in range(len(msg)):
            if msg[i].isdigit():
                begin = i
                break
        for i in range(len(msg) - 1, 0, -1):
            if msg[i].isdigit():
                end = i
                break
        msg = msg[begin:end+1].split(',')
        ip = '.'.join(msg[:4])
        port = int(msg[4]) * 256 + int(msg[5])
        errno = conn.connect_ex((ip, port))
        if errno != 0:
            print("Error:", errno)
            conn.close()
            conn = None
    except Exception as e:
        if conn:
            conn.close()
            conn = None
        raise e


def close_data():
    global data
    global conn
    global mode

    mode = MODE.NONE
    if data:
        try:
            data.close()
        except Exception as e:
            print(str(e))
        data = None
    if conn:
        try:
            conn.close()
        except Exception as e:
            print(str(e))
        conn = None


def send_command(word: str, interrupt=False):
    global sock
    if sock:
        try:
            sock.send((word + CRLF).encode())
            res, num = get_control_msg(interrupt)
            print_control_msg(res)
        except Exception as e:
            raise e
        return res, num


def get_transfer_data():
    global mode
    global data
    global conn
    global addr
    if mode == MODE.NONE:
        return 'No Connection Established'
    if mode == MODE.PORT:
        try:
            if not conn:
                conn, addr = data.accept()
                conn.setblocking(False)
            ready = select([conn], [], [], 1)
            if ready[0]:
                res = ready[0][0].recv(size)
                if len(res) == 0:
                    return 'Connection Closed'
                return res
            else:
                return 'Connection Timeout'
        except Exception as e:
            if str(e) == '[Errno 9] Bad file descriptor':
                print(str(e))
                print(
                    'The data connection has been closed. Ignore if transferred successfully.')
                return str(e)
            print(str(e))
            if isinstance(e, socket.timeout):
                return 'Connection Timeout'
            else:
                raise e
    elif mode == MODE.PASV:
        try:
            if conn:
                conn.setblocking(False)
                ready = select([conn], [], [], 1)
                if ready[0]:
                    res = ready[0][0].recv(size)
                    if len(res) == 0:
                        return 'Connection Closed'
                    return res
                else:
                    return 'Connection Timeout'
            return 'No Connection Established'
        except Exception as e:
            if str(e) == '[Errno 9] Bad file descriptor':
                print(str(e))
                print(
                    'The data connection has been closed. Ignore if transferred successfully.')
                return str(e)
            print(str(e))
            if isinstance(e, socket.timeout):
                return 'Connection Timeout'
            else:
                raise e
    else:
        return 'Unknown Mode'


def send_transfer_data(f):
    global mode
    global data
    global conn
    global addr
    if mode == MODE.NONE:
        raise Exception('No Connection Established')
    if mode == MODE.PORT:
        if not conn:
            conn, addr = data.accept()
        conn.sendfile(f)
    elif mode == MODE.PASV:
        conn.sendfile(f)
    else:
        raise Exception('Unknown Mode')


def print_transfer(more=False):
    while True:
        cur_data = get_transfer_data()
        if isinstance(cur_data, bytes):
            print(cur_data.decode(), end='')
        else:
            break
    close_data()
    if more:
        res, num = get_control_msg()
        print_control_msg(res)


def retrieve_file(filename: str, more=False):
    with open(filename, 'wb') as f:
        while True:
            cur_data = get_transfer_data()
            if isinstance(cur_data, bytes):
                f.write(cur_data)
            else:
                break
    close_data()
    if more:
        res, num = get_control_msg()
        print_control_msg(res)


def store_file(filename: str, more=False):
    f = open(filename, 'rb')
    send_transfer_data(f)
    close_data()
    if more:
        res, num = get_control_msg()
        print_control_msg(res)


cmd = ['CONN', 'LOCAL', 'INIT', 'EXIT',
       'QUIT', 'ABOR',
       'USER', 'PASS',
       'PORT', 'PASV',
       'SYST', 'TYPE',
       'LIST', 'NLST',
       'MKD', 'CWD', 'PWD', 'RMD',
       'RNFR', 'RNTO', 'DELE',
       'RETR', 'STOR'
       ]


def encode_path(path: str):
    return path.replace('\000', '\012')


if __name__ == "__main__":

    print('Welcome to FTP client!')

    while True:
        try:
            rawmsg = input().lstrip()
            msg = rawmsg.split()
            msg[0] = msg[0].upper()

            if msg[0] not in cmd:
                print('Unknown command')

            # THIS IS FOR DEBUGGING USE ONLY
            elif msg[0] == 'INIT':
                if msg[1] == '1':
                    connect_ftp('localhost', 21)
                send_command('USER anonymous')
                send_command('PASS anonymous')

            # CONNECT TO SERVER
            elif msg[0] == 'CONN':
                ip = msg[1]
                port = int(msg[2])
                connect_ftp(ip, port)

            # THIS IS FOR DEBUGGING USE ONLY
            # PRINT SOME CONNECTION INFO
            elif msg[0] == 'LOCAL':
                if not sock:
                    print('Not connected')
                else:
                    print(sock.getsockname())
                if not data:
                    print('No data transfer')
                else:
                    print(data.getsockname())
                if not conn:
                    print('No data transfer')
                else:
                    print(conn.getsockname())
                print(ip, port)
                print(user, pwd)
                print(mode)
                print(about_to_send, send_name)
                print(active_count())

            elif msg[0] == 'EXIT':
                close_socket()
                break

            elif not sock:
                print('Not connected')

            elif msg[0] == 'QUIT' or msg[0] == 'ABOR':
                close_socket(msg[0])

            elif msg[0] == 'USER':
                if len(msg) != 2:
                    print('Usage: USER <username>')
                    continue
                send_command('USER ' + rawmsg[5:])
                about_to_send = SENDTYPE.NONE
                send_name = None
                user = msg[1]

            elif msg[0] == 'PASS':
                if len(msg) != 2:
                    print('Usage: PASS <password>. use USER first')
                    continue
                res, num = send_command('PASS ' + rawmsg[5:])
                if res[-1][:3] == '230':
                    pwd = msg[1]
                else:
                    user = None
                    pwd = None

            elif msg[0] == 'PORT':
                if len(msg) != 2:
                    print('Usage: PORT <h1,h2,h3,h4,p1,p2>')
                    continue
                try:
                    open_port(msg[1])
                except Exception as e:
                    print(str(e))
                    continue
                res, num = send_command('PORT ' + msg[1])
                if res[-1][:3] != '200':
                    print('PORT failed')
                    close_data()
                else:
                    mode = MODE.PORT
                    if about_to_send == SENDTYPE.LIST:
                        data_thread = Thread(
                            target=print_transfer, daemon=True)
                        data_thread.start()
                        res, num = get_control_msg()
                        print_control_msg(res)
                        close_data()
                    elif about_to_send == SENDTYPE.RETR:
                        data_thread = Thread(
                            target=retrieve_file, args=(send_name, True, ), daemon=True)
                        data_thread.start()
                        res, num = get_control_msg()
                        print_control_msg(res)
                        close_data()

            elif msg[0] == 'PASV':
                res, num = send_command('PASV')
                if res[-1][:3] == '227':
                    try:
                        open_pasv(res[-1].split(' ')[-1])
                        if conn:
                            print('Data transfer connected')
                            mode = MODE.PASV
                    except Exception as e:
                        print(str(e))
                        continue
                    if about_to_send == SENDTYPE.LIST:
                        data_thread = Thread(
                            target=print_transfer, daemon=True)
                        data_thread.start()
                        res, num = get_control_msg()
                        print_control_msg(res)
                    elif about_to_send == SENDTYPE.RETR:
                        data_thread = Thread(
                            target=retrieve_file, args=(send_name, True, ), daemon=True)
                        data_thread.start()
                        res, num = get_control_msg()
                        print_control_msg(res)
                        close_data()
                else:
                    print('PASV failed')
                    close_data()

            elif msg[0] == 'LIST' or msg[0] == 'NLST':
                res, num = send_command(
                    msg[0] + ' ' + rawmsg[5:], interrupt=True)
                if res[0][:3] != '125' and res[0][:3] != '150':
                    close_data()
                elif mode != MODE.NONE:
                    data_thread = Thread(
                        target=print_transfer, args=(num == 1, ), daemon=True)
                    data_thread.start()
                elif num == 1 and res[0][0:3] == '150':
                    if res[0] == '150 File status okay. About to open data connection.':
                        about_to_send = SENDTYPE.LIST
                    else:
                        res, num = get_control_msg()
                        print_control_msg(res)

            elif msg[0] == 'TYPE':
                if len(msg) != 2 or msg[1] != 'I':
                    print('WE ONLY SUPPORT TYPE I')
                    continue
                send_command(msg[0] + ' ' + msg[1])

            elif msg[0] == 'MKD' or msg[0] == 'RMD':
                if len(msg) == 1:
                    print('Usage: {} <directory>'.format(msg[0]))
                    continue
                send_command(msg[0] + ' ' + encode_path(rawmsg[4:]))

            elif msg[0] == 'CWD':
                send_command(msg[0] + ' ' + encode_path(rawmsg[4:]))

            elif msg[0] == 'DELE' or msg[0] == 'RNFR' or msg[0] == 'RNTO':
                if len(msg) == 1:
                    print('Usage: {} <file>'.format(msg[0]))
                    continue
                send_command(msg[0] + ' ' + encode_path(rawmsg[5:]))

            elif msg[0] == 'RETR':
                if len(msg) == 1:
                    print('Usage: RETR <file>')
                    continue
                fname = rawmsg[5:]
                encodedname = encode_path(fname)
                try:
                    f = open(fname, 'wb')
                except Exception as e:
                    print(str(e))
                    f.close()
                    continue
                res, num = send_command(msg[0] + ' ' + encodedname, interrupt=True)
                if res[0][:3] != '125' and res[0][:3] != '150':
                    close_data()
                elif mode != MODE.NONE:
                    data_thread = Thread(target=retrieve_file,
                                         args=(fname, num == 1, ), daemon=True)
                    data_thread.start()
                elif num == 1 and res[0][0:3] == '150':
                    if res[0] == '150 File status okay. About to open data connection.':
                        about_to_send = SENDTYPE.RETR
                        send_name = fname
                    else:
                        res, num = get_control_msg()
                        print_control_msg(res)

            elif msg[0] == 'STOR':
                if len(msg) == 1:
                    print('Usage: STOR <file>')
                    continue
                fname = rawmsg[5:]
                encodedname = encode_path(fname)
                try:
                    f = open(fname, 'rb')
                except Exception as e:
                    print(str(e))
                    f.close()
                    continue
                res, num = send_command(msg[0] + ' ' + encodedname, interrupt=True)
                if res[0][:3] != '125' and res[0][:3] != '150':
                    close_data()
                elif mode != MODE.NONE:
                    data_thread = Thread(
                        target=store_file, args=(fname, num == 1, ), daemon=True)
                    data_thread.start()

            else:
                send_command(msg[0])

        except Exception as e:
            print(str(e))
            if str(e) == '[Errno 32] Broken pipe':
                close_socket()

    print('Bye')
