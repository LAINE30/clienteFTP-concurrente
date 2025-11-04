# Cliente FTP concurrente

Archivos:
- ZunigaJ-clienteFTP.c    : cliente FTP (ej. ZunigaJ)
- connectsock.c/.h        : helpers para conexión TCP
- errexit.c               : helper para errores
- Makefile

Compilar:
$ make

Ejecutar:
$ ./clienteftp <server> <port>
ejemplo:
$ ./clienteftp ftp.example.com 21

Uso interactivo (ejemplo):
ftp> user miusuario
ftp> pass miclave
ftp> pasv            # usar modo pasivo (por defecto)
ftp> port            # usar modo activo
ftp> retr archivo.txt
ftp> stor local.bin
ftp> quit

Cómo funciona:
- Mantiene una única conexión de control con el servidor FTP.
- Para cada STOR/RETR se crea un proceso hijo (fork). El hijo establece el canal de datos (PASV o PORT) y transfiere el archivo; el padre sigue en el control.
- Soporta tanto PASV como PORT. PASV es el modo por defecto.

Limitaciones / notas:
- No implementa reanudación (REST) ni manejo avanzado de timeouts.
- Asegúrate de que el servidor FTP permita conexiones pasivas/activas según tu red/NAT.
