# DevOps-02 — Ubuntu Container Cluster

Cluster de 3 nodos Ubuntu **dockerizados**, conectados por SSH, para practicar administración de Linux, redes y cómputo paralelo con **OpenMPI**.

> Aunque se les llama "VMs", técnicamente son **contenedores Docker** que se comportan como máquinas virtuales (SSH, usuarios, sudo, etc.).

## Estructura

```
devops-02 Ubuntu Container Cluster/
├── Dockerfile
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

## Referencia general

Para conceptos generales sobre `ENV`, `RUN`, `useradd` y endurecimiento SSH, ver:
[`3. Docker/ubuntu-vm/ubuntu-vm.md`](../../3.%20Docker/ubuntu-vm/ubuntu-vm.md)
# DevOps-02_Ubuntu-Container-Cluster
