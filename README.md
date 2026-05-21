# DevOps-02 — Ubuntu Container Cluster

Cluster de 3 nodos Ubuntu **dockerizados**, conectados por SSH, para practicar administración de Linux, redes y cómputo paralelo con **OpenMPI**.

> Aunque se les llama "VMs", técnicamente son **contenedores Docker** que se comportan como máquinas virtuales (SSH, usuarios, sudo, etc.).

## Estructura

```
devops-02 Ubuntu Container Cluster/
├── Dockerfile
├── docker-compose.yaml
├── entrypoint.sh           # arranca cada nodo según su rol (NFS server o client)
├── ssh_keys/
│   ├── hpckey
│   └── hpckey.pub
└── README.md
```

## Dockerfile

### Imagen base y variables de entorno

```dockerfile
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Bogota
```

- **`DEBIAN_FRONTEND=noninteractive`** — evita que `apt` haga preguntas interactivas durante el build.
- **`TZ=America/Bogota`** — zona horaria por defecto (la usa `tzdata` al instalarse).

### Paquetes instalados

```dockerfile
RUN apt-get update && apt-get install -y \
    openssh-server \
    openmpi-bin openmpi-common libopenmpi-dev \
    gcc make \
    net-tools iputils-ping \
    sudo nano \
    nfs-kernel-server nfs-common rpcbind \
 && rm -rf /var/lib/apt/lists/*
```

| Paquete                                          | Para qué sirve                                                        |
| ------------------------------------------------ | --------------------------------------------------------------------- |
| `openssh-server`                                 | Servidor SSH (login remoto entre nodos)                               |
| `openmpi-bin`, `openmpi-common`, `libopenmpi-dev`| Cómputo paralelo con MPI                                              |
| `gcc`, `make`                                    | Compilar programas MPI en C                                           |
| `net-tools`, `iputils-ping`                      | Diagnóstico de red                                                    |
| `sudo`, `nano`                                   | Administración y edición                                              |
| `nfs-kernel-server`                              | Demonio del servidor NFS (lo usa **node1** para exportar la carpeta)  |
| `nfs-common`                                     | Cliente NFS + utilidades (`mount.nfs`, `rpcinfo`, etc.)               |
| `rpcbind`                                        | Mapeador de puertos RPC requerido por NFS                             |

### Endurecimiento SSH

```dockerfile
RUN mkdir -p /var/run/sshd \
 && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin no/' /etc/ssh/sshd_config \
 && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config
```

- **`PermitRootLogin no`** — bloquea login del usuario `root` por SSH.
- **`PasswordAuthentication no`** — desactiva login por contraseña; solo se permite **clave pública**.

### Usuario `mpiuser`

```dockerfile
RUN useradd -m -s /bin/bash mpiuser \
 && echo "mpiuser ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER mpiuser
WORKDIR /home/mpiuser
```

- Se usa **`useradd`** (no `adduser`) porque no es interactivo.
- `-m` crea el `home`, `-s /bin/bash` define la shell.
- Se le da `sudo` sin contraseña (`NOPASSWD: ALL`).
- A partir de aquí, el contenedor opera como `mpiuser`.

### Claves SSH del usuario

```dockerfile
RUN mkdir -p /home/mpiuser/.ssh \
 && chmod u=rwx,go= /home/mpiuser/.ssh

COPY --chown=mpiuser:mpiuser ssh_keys/hpckey       /home/mpiuser/.ssh/hpckey
COPY --chown=mpiuser:mpiuser ssh_keys/hpckey.pub   /home/mpiuser/.ssh/hpckey.pub
COPY --chown=mpiuser:mpiuser ssh_keys/hpckey.pub   /home/mpiuser/.ssh/authorized_keys

RUN printf "Host *\n\tStrictHostKeyChecking no\n\tUserKnownHostsFile=/dev/null\n" \
        > /home/mpiuser/.ssh/config \
 && chmod u=rw,go= /home/mpiuser/.ssh/config
```

- Se copian las claves (`hpckey`, `hpckey.pub`) generadas localmente.
- La pública se agrega a `authorized_keys` → cada nodo confía en quien tenga `hpckey`.
- El `config` desactiva la verificación de `known_hosts` para que los nodos se conecten entre sí sin preguntar.

### Entrypoint

```dockerfile
USER root
EXPOSE 22 2049 111

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
```

- Se exponen los puertos **22** (SSH), **2049** (NFS) y **111** (rpcbind).
- En vez de un `CMD` fijo con `sshd`, el contenedor arranca con **`entrypoint.sh`**, que decide su comportamiento según la variable de entorno `NODE_ROLE` (ver sección [`entrypoint.sh`](#entrypointsh)).

## entrypoint.sh

Un único script que se comparte entre los 3 nodos. Lo que ejecuta depende de `NODE_ROLE`:

- **`NODE_ROLE=server`** → arranca el servidor NFS (configura `/etc/exports`, levanta `rpcbind` y `nfs-kernel-server`, ejecuta `exportfs -ra`).
- **`NODE_ROLE=client`** → espera a que el puerto 2049 del `$NFS_SERVER` responda y monta `172.30.0.11:/` en `/mnt/cluster` vía NFSv4 (la raíz NFSv4 es `/mnt/cluster` gracias a `fsid=0`).
- Al final, **todos** los nodos arrancan `sshd -D` en foreground (para que el contenedor no se cierre).

```bash
#!/bin/bash
set -e

mkdir -p /mnt/cluster
chown mpiuser:mpiuser /mnt/cluster

if [ "$NODE_ROLE" = "server" ]; then
    echo "/mnt/cluster 172.30.0.0/16(rw,sync,fsid=0,no_subtree_check,no_root_squash)" > /etc/exports
    service rpcbind start
    service nfs-kernel-server start
    exportfs -ra

elif [ "$NODE_ROLE" = "client" ]; then
    for i in $(seq 1 30); do
        if timeout 1 bash -c "</dev/tcp/$NFS_SERVER/2049" 2>/dev/null; then
            break
        fi
        sleep 2
    done
    mount -t nfs4 "$NFS_SERVER:/" /mnt/cluster
fi

exec /usr/sbin/sshd -D
```

### Opciones de `/etc/exports`

`/mnt/cluster 172.30.0.0/16(rw,sync,fsid=0,no_subtree_check,no_root_squash)`

| Opción              | Significado                                                                     |
| ------------------- | ------------------------------------------------------------------------------- |
| `172.30.0.0/16`     | Solo clientes de la subred del cluster pueden montar                            |
| `rw`                | Lectura y escritura                                                             |
| `sync`              | Escrituras confirmadas en disco antes de responder (más lento pero seguro)      |
| `fsid=0`            | Hace que `/mnt/cluster` sea la **raíz** del pseudo-FS NFSv4 → clientes montan `server:/` |
| `no_subtree_check`  | Desactiva la verificación de subárbol (mejor rendimiento y robustez)            |
| `no_root_squash`    | El `root` del cliente se trata como `root` real (necesario para `mpirun` y SSH) |

## docker-compose.yaml

Levanta los **3 nodos** del cluster (`node1`, `node2`, `node3`) a partir de la imagen construida desde el `Dockerfile`. Cada nodo tiene su **IP fija** dentro de una red bridge y comparte una carpeta vía **NFS** — `node1` actúa como servidor y los otros dos como clientes.

```yaml
services:
  node1:
    build: .
    image: mpi-node:latest
    container_name: node1
    privileged: true
    environment:
      NODE_ROLE: server
    volumes:
      - nfs-data:/mnt/cluster
    networks:
      my-red:
        ipv4_address: 172.30.0.11

  node2:
    image: mpi-node:latest
    container_name: node2
    privileged: true
    environment:
      NODE_ROLE: client
      NFS_SERVER: 172.30.0.11
    depends_on:
      - node1
    networks:
      my-red:
        ipv4_address: 172.30.0.12

  node3:
    image: mpi-node:latest
    container_name: node3
    privileged: true
    environment:
      NODE_ROLE: client
      NFS_SERVER: 172.30.0.11
    depends_on:
      - node1
    networks:
      my-red:
        ipv4_address: 172.30.0.13

volumes:
  nfs-data:

networks:
  my-red:
    driver: bridge
    ipam:
      config:
        - subnet: 172.30.0.0/16
```

### Volumen `nfs-data`

Named volume Docker montado **solo en `node1`** sobre `/mnt/cluster`. Es el "disco físico" del servidor NFS: aquí viven realmente los archivos. `node2` y `node3` **no** tienen este volumen — acceden al contenido vía `mount -t nfs4` por la red.

Razón técnica: el filesystem `overlay2` (default de los contenedores) no soporta exports NFS porque no implementa file-handles. Un named volume Docker es ext4 en la VM, sí lo soporta. Equivale a que un servidor HPC real tenga sus datos en un disco ext4/xfs local antes de exportarlos por NFS.

### Servicios

| Servicio | Imagen            | IP fija         | Rol NFS  | Notas                                                |
| -------- | ----------------- | --------------- | -------- | ---------------------------------------------------- |
| `node1`  | `mpi-node:latest` | `172.30.0.11`   | server   | **`build: .`** → construye la imagen aquí. Head MPI. |
| `node2`  | `mpi-node:latest` | `172.30.0.12`   | client   | Monta `172.30.0.11:/mnt/cluster` al arrancar.        |
| `node3`  | `mpi-node:latest` | `172.30.0.13`   | client   | Monta `172.30.0.11:/mnt/cluster` al arrancar.        |

- **`build: .`** solo en `node1` → la imagen `mpi-node:latest` se construye una vez; los otros nodos la reutilizan.
- **`container_name`** → fija el nombre del contenedor (útil para `docker exec -it node1 bash`).
- **`privileged: true`** → necesario en los 3:
  - `node1` necesita permisos del kernel para correr `nfs-kernel-server`.
  - `node2`/`node3` necesitan poder hacer `mount` dentro del contenedor.
- **`environment`** → define el rol que leerá `entrypoint.sh`:
  - `NODE_ROLE` decide si arranca como `server` o `client`.
  - `NFS_SERVER` solo en clientes; apunta a la IP fija de `node1`.
- **`depends_on: [node1]`** → garantiza que `node1` arranque primero. El loop de espera del entrypoint cubre el resto (el `depends_on` solo espera al *contenedor*, no al servicio NFS dentro).

### Red — `my-red`

```yaml
networks:
  my-red:
    driver: bridge
    ipam:
      config:
        - subnet: 172.30.0.0/16
```

- **`driver: bridge`** → red interna aislada para los contenedores del proyecto.
- **`subnet: 172.30.0.0/16`** → rango de IPs disponible.
- Cada nodo recibe una **IP fija** (`ipv4_address`) para poder referenciarse entre sí (ej. `ssh mpiuser@172.30.0.12`).

### Almacenamiento compartido vía NFS

A diferencia de un bind mount al host, aquí los datos viven en un **named volume Docker (`nfs-data`)** montado solo en `node1` sobre `/mnt/cluster`. `node1` exporta esa ruta por NFS y los demás nodos la montan por red:

```
host
└── (sin carpeta compartida en el host)

nfs-data (volumen Docker, ext4 en la VM)
    │
    └─→ node1  ── /mnt/cluster (real, exportado por NFS)
                    ▲
                    │  NFSv4
                    │
        node2  ── /mnt/cluster (montado del server)
        node3  ── /mnt/cluster (montado del server)
```

### Ubicación dentro del contenedor

`/mnt/cluster` está en la **raíz del filesystem**, no dentro del home del usuario. Al entrar al contenedor con `docker exec -it node1 bash`, caes en `/home/mpiuser` (`~`), y desde ahí **no verás** la carpeta haciendo `ls`. Para acceder:

```bash
ls -la /mnt/cluster      # ruta absoluta
cd /mnt/cluster          # o cambiarte ahí directamente
```

Layout del filesystem dentro de cada nodo:

```
/
├── home/
│   └── mpiuser/         ← home del usuario (~), aquí caes al entrar
│       ├── .bashrc
│       └── .ssh/
└── mnt/
    └── cluster/         ← montaje NFS compartido entre los 3 nodos
```

### Notas

- Los **3 nodos ven la misma ruta** `/mnt/cluster`, así que `mpirun` puede asumir que el binario y los datasets existen igual en cada uno.
- Para meter archivos al cluster desde el host, se usan `docker cp` o `scp` contra `node1`:
  ```bash
  docker cp matmul.c node1:/mnt/cluster/
  ```
- Como `/mnt/cluster` está respaldado por el volumen `nfs-data`, los datos **sobreviven** a `docker compose down`. Para borrarlos también, usa `docker compose down -v`.

### Comandos útiles

```bash
# Construir y levantar los 3 nodos (la imagen ya trae mul.c y script.sh
# desde la carpeta mpi/; al arrancar node1 compila con gcc -Ofast y los
# deja en /mnt/cluster, visibles por NFS en los 3 nodos)
docker compose up -d --build

# Entrar a node1 como mpiuser
docker exec -it -u mpiuser node1 bash

# Una vez dentro, ejecutar el script de pruebas
cd /mnt/cluster
./script.sh
cat times2.txt        # tiempos generados

# Verificar que el NFS está montado en los clientes
docker exec node2 mount | grep cluster
# → 172.30.0.11:/ on /mnt/cluster type nfs4 ...

# Probar la sincronización: escribe en node1, lee desde node3
docker exec node1 bash -c "echo hola > /mnt/cluster/test.txt"
docker exec node3 cat /mnt/cluster/test.txt

# Ver los exports activos en el server
docker exec node1 exportfs -v

# Detener y eliminar todo
docker compose down
```

### Archivos MPI (carpeta `mpi/`)

| Archivo         | Qué hace                                                                                                                                                |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mpi/mul.c`     | Multiplicación de matrices distribuida con MPI. Cada rank lee **su slice** de `A` y `B` desde disco; solo se mide cómputo + `MPI_Gatherv`.              |
| `mpi/gen.c`     | Generador de matrices. Escribe `A_<n>.bin` y `B_<n>.bin` (binarios `int`) con `srand(42 + n)` (semilla fija → reproducible).                            |
| `mpi/script.sh` | Ejecuta `mpirun --map-by node -np 3` sobre 9 tamaños, 10 repeticiones cada uno, y guarda los tiempos en `times2.txt`.                                   |
| `mpi/hostfile`  | Lista de nodos con sus slots para `mpirun` (`node1 slots=4`, `node2 slots=4`, `node3 slots=4`).                                                         |

El `Dockerfile` copia `mpi/` a `/opt/mpi/` dentro de la imagen. `entrypoint.sh` (solo en `node1`) compila ambos binarios (`mpicc -Ofast` para `mul`, `gcc -Ofast` para `gen`), y **genera las 9 matrices** (`n` ∈ {1000, 1259, 1586, 1996, 2513, 3163, 3981, 5010, 6310}) en `/mnt/cluster/matrices/`. Como esta carpeta vive en el volumen `nfs-data`, las matrices **se generan una sola vez** y se reutilizan en arranques posteriores.

### Por qué matrices pre-generadas

Generar las matrices en cada corrida (con `rand()` sobre rank 0) y luego difundirlas con `MPI_Bcast` introduce dos costes que distorsionan la medición: tiempo de CPU para `rand()` y tráfico de red para el broadcast. Al tener las matrices en disco compartido vía NFS, **cada rank lee solo su porción** (`fseek` + `fread`) y la medición refleja únicamente el cómputo paralelo.

### Cómo distribuye el trabajo MPI

1. `mpirun --map-by node -np 3` lanza un proceso (`rank`) en cada nodo.
2. Cada rank calcula sus `sendcounts`/`displs` y abre `A_<n>.bin` haciendo `fseek` a su porción → lee solo sus filas.
3. Cada rank lee `B_<n>.bin` completa (la necesita entera para multiplicar).
4. **`MPI_Barrier` + `MPI_Wtime()`** → empieza la medición.
5. Cada rank calcula sus filas de `C = A_local * B`.
6. `MPI_Gatherv(C)` → rank 0 recolecta `C` completa.
7. **`MPI_Barrier` + `MPI_Wtime()`** → fin de medición. Rank 0 imprime `n tiempo`.

Variables del script (override con env vars):

```bash
NP=6 ./script.sh          # 6 procesos (2 por nodo)
NP=3 OUT=/mnt/cluster/run1.txt ./script.sh
```

## MPI explicado fácil

### Cómo `mpirun` encuentra los otros nodos

1. **`hostfile`** dice los nombres: `node1`, `node2`, `node3`.
2. **Docker Compose** crea una DNS interna en la red `my-red` → esos nombres resuelven a `172.30.0.11`, `.12`, `.13`.
3. **SSH con llaves** (`~/.ssh/hpckey`) permite a `mpirun` arrancar procesos en los otros nodos sin contraseña.
4. Una vez arrancados, los procesos hablan entre sí por **TCP** dentro de la red Docker.

Resumen: `hostfile` (nombres) + Docker DNS (resolver) + SSH (autenticación) = `mpirun` puede ejecutar código en los 3 contenedores.

### Las 6 funciones MPI que usa el código

| Función | Qué hace |
| ------- | -------- |
| `MPI_Init(&argc, &argv)` | Arranca MPI. **Siempre la primera** llamada. |
| `MPI_Comm_rank(comm, &rank)` | Devuelve **mi número** dentro del grupo (0, 1 o 2). |
| `MPI_Comm_size(comm, &size)` | Devuelve **cuántos procesos** hay en total (3). |
| `MPI_Barrier(comm)` | Espera a que **todos** lleguen a este punto antes de seguir. Útil antes y después de cronometrar. |
| `MPI_Wtime()` | Devuelve un tiempo en segundos. Restando dos llamadas se mide cuánto tardó algo. |
| `MPI_Gatherv(...)` | **Junta** trozos de los demás ranks en un solo arreglo del rank 0. |
| `MPI_Finalize()` | Cierra MPI. **Siempre la última** llamada. |

### Flujo del programa en una imagen

```
mpirun --map-by node -np 3 /mnt/cluster/mul 1000
        │
        ├─→ node1 (rank 0): lee filas 0-333 de A_1000.bin
        ├─→ node2 (rank 1): lee filas 334-666 de A_1000.bin
        └─→ node3 (rank 2): lee filas 667-999 de A_1000.bin
                │
        cada uno carga B_1000.bin entera
                │
        MPI_Barrier ── todos esperan ──
                │
        cada uno multiplica su trozo en paralelo
                │
        MPI_Gatherv ── envían sus trozos a rank 0 ──
                │
        MPI_Barrier ── todos esperan ──
                │
        rank 0 imprime "1000 0.034"
```

**La idea clave:** el mismo binario corre 3 veces, pero como `rank` es distinto en cada uno, cada copia procesa una parte diferente del problema.

## Referencia general

Para conceptos generales sobre `ENV`, `RUN`, `useradd` y endurecimiento SSH, ver:
[`3. Docker/ubuntu-vm/ubuntu-vm.md`](../../3.%20Docker/ubuntu-vm/ubuntu-vm.md)
