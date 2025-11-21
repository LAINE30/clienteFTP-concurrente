# **README – Cliente FTP Concurrente**

## **Descripción general**

Este proyecto implementa un **cliente FTP concurrente en C**, capaz de:

* Realizar **múltiples transferencias de archivos simultáneamente** (en ambos sentidos).
* Crear un **fork() por cada comando de transferencia**:

  * El **proceso hijo** realiza el RETR/STOR/LIST.
  * El **padre** continúa atendiendo comandos del usuario.
* Funcionar en **modo PASV (pasivo)**.

Este cliente permite que el usuario lance varias descargas/cargas a la vez, sin bloquear la interfaz.

---

# **Archivos del proyecto**

| Archivo                           | Descripción                              |
| --------------------------------- | ---------------------------------------- |
| **CoronadoL-clienteFTP.c**        | Utilidades para crear sockets TCP.       |
| **Makefile**                      | Compilación del proyecto.                |

---

# **Compilación**

En Linux (incluido WSL):

```bash
make
```

Esto generará:

```
./CoronadoL-clienteFTP
```

---

# **Ejecución**

Ejemplo:

```bash
./CoronadoL-clienteFTP <servidor>
```

Ejemplo contra localhost:

```bash
./clienteFTP localhost
```

---

# **Uso interactivo**

Ejemplo de sesión:

```
ftp> user miusuario
ftp> pass miclave
ftp> retr archivo1.txt
ftp> retr archivo2.txt
ftp> stor subir.bin
ftp> dir
ftp> quit
```

Mientras una descarga está en progreso, puedes ejecutar otros comandos sin esperar.

---

# **Funcionamiento interno**

## **1. Conexión de control**

* Se abre un socket TCP hacia el puerto del servidor FTP.
* Todas las órdenes FTP viajan por aquí.

## **2. Concurrencia con fork()**

Cada vez que el usuario ejecuta un comando que requiere canal de datos:

* `RETR` (descargar)
* `STOR` (subir)
* `LIST` / `NLST`

Se hace lo siguiente:

1. **fork()**
2. **Hijo:**

   * Establece el canal de datos (PASV o PORT).
   * Ejecuta la transferencia completa.
   * Cierra archivos, cierra socket de datos, termina.
3. **Padre:**

   * No espera al hijo (no hace wait).
   * Regresa inmediatamente al prompt.
   * Puede seguir aceptando comandos.

Esto hace que el cliente sea **verdaderamente concurrente**.

---

# **Modos de transferencia**


# **Configuración del servidor FTP (ejemplo: vsftpd)**

Este proyecto fue probado con el servido de linux **vsftpd 3.x**.

## **Instalación en Ubuntu / WSL**

```bash
sudo apt update
sudo apt install vsftpd
```

## **Archivo de configuración**

Editar:

```bash
sudo nano /etc/vsftpd.conf
```

Recomendación mínima en el servidor:

```
listen=YES
listen_ipv6=NO
anonymous_enable=NO
local_enable=YES
write_enable=YES
dirmessage_enable=YES
use_localtime=YES
xferlog_enable=YES

```

Reiniciar:

```bash
sudo service vsftpd restart
```

---

# **Cómo probar el cliente concurrentemente**

## **Método 1: WSL como cliente**

En Ubuntu:

```
make
```

Luego prueba el cliente:

```bash
./CoronadoL-clienteFTP localhost
```

No debe bloquear si inicia seción desde otra parte.


# **Limitaciones**

* No implementa:

  * REST (reanudación)
  * TLS/SSL (FTPS)
  * Comandos avanzados de FTP
* No maneja timeouts sofisticados.

---
