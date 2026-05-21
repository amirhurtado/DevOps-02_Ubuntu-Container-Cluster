FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Bogota

RUN apt-get update && apt-get install -y \
    openssh-server \
    openmpi-bin \
    openmpi-common \
    libopenmpi-dev \
    gcc \
    make \
    net-tools \
    iputils-ping \
    sudo \
    nano \
    nfs-kernel-server \
    nfs-common \
    rpcbind \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/run/sshd \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin no/' /etc/ssh/sshd_config \
    && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config

RUN useradd -m -s /bin/bash mpiuser \
    && echo "mpiuser ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers


USER mpiuser
WORKDIR /home/mpiuser

RUN mkdir -p /home/mpiuser/.ssh \
    && chmod u=rwx,go= /home/mpiuser/.ssh

COPY --chown=mpiuser:mpiuser ssh_keys/hpckey /home/mpiuser/.ssh/hpckey
COPY --chown=mpiuser:mpiuser ssh_keys/hpckey.pub /home/mpiuser/.ssh/hpckey.pub
COPY --chown=mpiuser:mpiuser ssh_keys/hpckey.pub /home/mpiuser/.ssh/authorized_keys

RUN printf "Host *\n\tStrictHostKeyChecking no\n\tUserKnownHostsFile=/dev/null\n" > /home/mpiuser/.ssh/config \
    && chmod u=rw,go= /home/mpiuser/.ssh/config

USER root
EXPOSE 22 2049 111

COPY --chown=mpiuser:mpiuser mpi/ /opt/mpi/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]