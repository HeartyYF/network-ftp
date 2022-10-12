homework use, but due to some reason homework was cancelled

# Usage

```
cd server
make
./server -r/-root <root dir, /tmp by default> -p/-port <port number, 21 by default> -c <max connected clients, 10 by default>
```

Can compile and run on macos and linux, but due to course reason need gcc-12 on mac. You can modify makefile or compile using other compiler. These .hpp files are just to imply they contain implementations along with definitions, not to show that they are cpp.

```
python client.py
conn ip port
(other commands)
exit
```

You can read the source and find out other usages.
