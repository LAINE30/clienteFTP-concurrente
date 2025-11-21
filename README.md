Aquí tienes un **README completo, limpio y listo para copiar y descargar**.
No incluye código fuente, solo la documentación detallada del proyecto.

---

# **README – Cliente FTP Concurrente**

## **Descripción general**

Este proyecto implementa un **cliente FTP concurrente en C**, capaz de:

* Mantener **una sola conexión de control** con el servidor FTP.
* Realizar **múltiples transferencias de archivos simultáneamente** (en ambos sentidos).
* Crear un **fork() por cada comando de transferencia**:

  * El **proceso hijo** realiza el RETR/STOR/LIST.
  * El **padre** continúa atendiendo comandos del usuario.
* Funcionar tanto en **modo PASV (pasivo)** como **PORT (activo)**.

Este cliente permite que el usuario lance varias descargas/cargas a la vez, sin bloquear la interfaz.

---

# **Archivos del proyecto**

| Archivo                           | Descripción                              |
| --------------------------------- | ---------------------------------------- |
| **clienteFTP.c**                  | Cliente FTP concurrente.                 |
| **connectsock.c / connectsock.h** | Utilidades para crear sockets TCP.       |
| **errexit.c**                     | Función auxiliar para manejo de errores. |
| **Makefile**                      | Compilación del proyecto.                |

---

# **Compilación**

En Linux (incluido WSL):

```bash
make
```

Esto generará:

```
./clienteftp
```

---

# **Ejecución**

Ejemplo:

```bash
./clienteftp <servidor> <puerto>
```

Ejemplo contra localhost:

```bash
./clienteftp 127.0.0.1 21
```

---

# **Uso interactivo**

Ejemplo de sesión:

```
ftp> user miusuario
ftp> pass miclave
ftp> pasv        # activar modo pasivo (por defecto)
ftp> port        # activar modo activo
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

* Se abre un socket TCP hacia el puerto 21 del servidor FTP.
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

## **PASV (Pasivo — recomendado)**

El servidor abre un puerto y el cliente se conecta a él.

Ideal para:

✔ NAT
✔ WiFi compartida
✔ Probar desde WSL hacia Windows

## **PORT (Activo)**

El cliente abre un puerto y el servidor se conecta a él.

⚠ Puede fallar si tienes firewall o NAT estricto.

---

# **Configuración del servidor FTP (ejemplo: vsftpd)**

Este proyecto fue probado con **vsftpd 3.x**.

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

Recomendación mínima:

```
listen=YES
listen_ipv6=NO
anonymous_enable=NO
local_enable=YES
write_enable=YES
dirmessage_enable=YES
use_localtime=YES
xferlog_enable=YES

# Para permitir modo PASV
pasv_enable=YES
pasv_min_port=40000
pasv_max_port=40100

# Para permitir modo activo (PORT)
port_enable=YES
```

Reiniciar:

```bash
sudo service vsftpd restart
```

---

# **Cómo probar el cliente concurrentemente**

## **Método 1: WSL como servidor, Windows como cliente**

En Windows PowerShell:

```powershell
ftp localhost
```

Luego prueba tu cliente:

```bash
./clienteftp 127.0.0.1 21
```

## **Método 2: Varias transferencias simultáneas**

En el cliente:

```
ftp> retr file1.bin
ftp> retr file2.iso
ftp> retr file3.tar.gz
```

No debe bloquear.

Usa `ps` en WSL para ver los hijos:

```bash
ps -f | grep clienteftp
```

Debes ver varios procesos en paralelo.

---

# **Limitaciones**

* No implementa:

  * REST (reanudación)
  * TLS/SSL (FTPS)
  * Comandos avanzados de FTP
* No maneja timeouts sofisticados.

---

# **Licencia**

Proyecto libre para uso académico.

---

# **Contacto**

Si deseas agregar FTPS, threads en lugar de fork(), o soporte completo para IPv6, puedo ayudarte.

---

Si quieres, te genero también un **PDF descargable** con este README. Would you like that?
