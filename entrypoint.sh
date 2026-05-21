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

    echo "[entrypoint] Copiando fuentes y compilando con mpicc -Ofast"
    cp /opt/mpi/mul.c /mnt/cluster/mul.c
    cp /opt/mpi/gen.c /mnt/cluster/gen.c
    cp /opt/mpi/script.sh /mnt/cluster/script.sh
    cp /opt/mpi/hostfile /mnt/cluster/hostfile
    chmod +x /mnt/cluster/script.sh
    mpicc -Ofast /mnt/cluster/mul.c -o /mnt/cluster/mul
    gcc  -Ofast /mnt/cluster/gen.c -o /mnt/cluster/gen

    mkdir -p /mnt/cluster/matrices
    for N in 1000 1259 1586 1996 2513 3163 3981 5010 6310; do
        A=/mnt/cluster/matrices/A_${N}.bin
        B=/mnt/cluster/matrices/B_${N}.bin
        if [ ! -f "$A" ] || [ ! -f "$B" ]; then
            echo "[entrypoint] Generando matrices n=${N}"
            /mnt/cluster/gen "$N" "$A" "$B"
        else
            echo "[entrypoint] Matrices n=${N} ya existen, se omiten"
        fi
    done

    chown -R mpiuser:mpiuser /mnt/cluster
    echo "[entrypoint] Contenido de /mnt/cluster:"
    ls -la /mnt/cluster
    echo "[entrypoint] Contenido de /mnt/cluster/matrices:"
    ls -la /mnt/cluster/matrices

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
