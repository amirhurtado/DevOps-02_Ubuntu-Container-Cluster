#!/bin/bash
set -e

mkdir -p /mnt/cluster
chown mpiuser:mpiuser /mnt/cluster

if [ "$NODE_ROLE" = "server" ]; then
    echo "[entrypoint] Iniciando como NFS server"

    echo "/mnt/cluster 172.30.0.0/16(rw,sync,fsid=0,no_subtree_check,no_root_squash)" > /etc/exports

    service rpcbind start
    service nfs-kernel-server start
    exportfs -ra

    echo "[entrypoint] Exports activos:"
    exportfs -v

    echo "[entrypoint] Copiando fuentes y compilando con gcc -Ofast"
    cp /opt/mpi/mul.c /mnt/cluster/mul.c
    cp /opt/mpi/script.sh /mnt/cluster/script.sh
    chmod +x /mnt/cluster/script.sh
    gcc -Ofast /mnt/cluster/mul.c -o /mnt/cluster/mul
    chown -R mpiuser:mpiuser /mnt/cluster
    echo "[entrypoint] Contenido de /mnt/cluster:"
    ls -la /mnt/cluster

elif [ "$NODE_ROLE" = "client" ]; then
    echo "[entrypoint] Iniciando como NFS client (server=$NFS_SERVER)"

    for i in $(seq 1 30); do
        if timeout 1 bash -c "</dev/tcp/$NFS_SERVER/2049" 2>/dev/null; then
            echo "[entrypoint] NFS server disponible"
            break
        fi
        echo "[entrypoint] Esperando NFS server... ($i/30)"
        sleep 2
    done

    mount -t nfs4 "$NFS_SERVER:/" /mnt/cluster
    echo "[entrypoint] Montaje NFS hecho:"
    mount | grep cluster

else
    echo "[entrypoint] NODE_ROLE no definido, arrancando solo sshd"
fi

exec /usr/sbin/sshd -D
