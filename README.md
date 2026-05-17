# DevOps-02 — Ubuntu Container Cluster

Cluster de 3 nodos Ubuntu **dockerizados**, conectados por SSH, para practicar administración de Linux, redes y cómputo paralelo con **OpenMPI**.

> Aunque se les llama "VMs", técnicamente son **contenedores Docker** que se comportan como máquinas virtuales (SSH, usuarios, sudo, etc.).

## Estructura

```
devops-02 Ubuntu Container Cluster/
├── Dockerfile
├── docker-compose.yaml
├── shared/                 # volumen compartido entre los 3 nodos
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
 && rm -rf /var/lib/apt/lists/*
```

| Paquete                                  | Para qué sirve                          |
| ---------------------------------------- | --------------------------------------- |
| `openssh-server`                         | Servidor SSH (login remoto entre nodos) |
| `openmpi-bin`, `openmpi-common`, `libopenmpi-dev` | Cómputo paralelo con MPI       |
| `gcc`, `make`                            | Compilar programas MPI en C             |
| `net-tools`, `iputils-ping`              | Diagnóstico de red                      |
| `sudo`, `nano`                           | Administración y edición                |

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

## docker-compose.yaml

Levanta los **3 nodos** del cluster (`node1`, `node2`, `node3`) a partir de la imagen construida desde el `Dockerfile`. Cada nodo tiene su **IP fija** dentro de una red bridge y comparte un volumen para intercambiar archivos.

```yaml
services:
  node1:
    build: .
    image: mpi-node:latest
    container_name: node1
    networks:
      my-red:
        ipv4_address: 172.20.0.11
    volumes:
      - ./shared:/mnt/cluster

  node2:
    image: mpi-node:latest
    container_name: node2
    networks:
      my-red:
        ipv4_address: 172.20.0.12
    volumes:
      - ./shared:/mnt/cluster

  node3:
    image: mpi-node:latest
    container_name: node3
    networks:
      my-red:
        ipv4_address: 172.20.0.13
    volumes:
      - ./shared:/mnt/cluster

networks:
  my-red:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/16
```

### Servicios

| Servicio | Imagen            | IP fija         | Notas                                       |
| -------- | ----------------- | --------------- | ------------------------------------------- |
| `node1`  | `mpi-node:latest` | `172.20.0.11`   | **`build: .`** → construye la imagen aquí   |
| `node2`  | `mpi-node:latest` | `172.20.0.12`   | Reutiliza la imagen ya construida           |
| `node3`  | `mpi-node:latest` | `172.20.0.13`   | Reutiliza la imagen ya construida           |

- **`build: .`** solo en `node1` → la imagen `mpi-node:latest` se construye una vez; los otros nodos la reutilizan.
- **`container_name`** → fija el nombre del contenedor (útil para `docker exec -it node1 bash`).

### Red — `my-red`

```yaml
networks:
  my-red:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/16
```

- **`driver: bridge`** → red interna aislada para los contenedores del proyecto.
- **`subnet: 172.20.0.0/16`** → rango de IPs disponible.
- Cada nodo recibe una **IP fija** (`ipv4_address`) para poder referenciarse entre sí (ej. `ssh mpiuser@172.20.0.12`).

### Volumen compartido

```yaml
volumes:
  - ./shared:/mnt/cluster
```

- Monta la carpeta local `./shared` dentro de cada contenedor en `/mnt/cluster`.
- Los **3 nodos ven el mismo directorio**, lo que permite compartir programas MPI, datasets y resultados sin copiarlos por SSH.

### Comandos útiles

```bash
# Construir y levantar los 3 nodos
docker compose up -d

# Entrar a un nodo
docker exec -it node1 bash

# Detener y eliminar todo
docker compose down
```

## Referencia general

Para conceptos generales sobre `ENV`, `RUN`, `useradd` y endurecimiento SSH, ver:
[`3. Docker/ubuntu-vm/ubuntu-vm.md`](../../3.%20Docker/ubuntu-vm/ubuntu-vm.md)
# DevOps-02_Ubuntu-Container-Cluster
