# Readiness probes for WeSQL

This note says when a WeSQL server is ready for writes and how to probe it
from Docker Compose and Kubernetes.

## What changed

Images built before the Raft removal (for example
`apecloud/wesql-server:8.0.35-0.1.0_beta5.40`) opened the MySQL port before
the member had been elected leader and had caught up. A write in that window
failed with:

```
ERROR 1598 (HY000): Binary logging not possible. Message: Consensus Not Leader
```

`mysqladmin ping` and a TCP check both succeed in that window, so they are
not readiness probes for those images. Use this query instead; the member is
ready when it returns `Leader Yes`:

```sql
SELECT ROLE, SERVER_READY_FOR_RW FROM INFORMATION_SCHEMA.WESQL_CLUSTER_LOCAL;
```

The current `8.0` branch runs a single node without Raft. Object store
recovery (snapshot, InnoDB, SmartEngine and binlog download) runs inside
`init_server_components()`, and mysqld serves client connections only after
it has finished and the binlog and snapshot archive threads have started.
`INFORMATION_SCHEMA.WESQL_CLUSTER_LOCAL` no longer exists.

## `Wesql_ready_for_write`

The global status variable `Wesql_ready_for_write` is `ON` when:

- the server has finished starting;
- `read_only` and `super_read_only` are both `OFF`;
- when `serverless`, `log_bin` and `binlog_archive` are all `ON`, the binlog
  archive thread that ships committed binlog to the object store is running.

```sql
SHOW GLOBAL STATUS LIKE 'Wesql_ready_for_write';
```

A server that answers the query has finished recovery, because recovery runs
before connections are served. The variable turns `OFF` again if the server
is made read-only, or if the binlog archive thread stops. While that thread is
stopped, commits still succeed but are not copied to the object store.

## Docker Compose

```yaml
    healthcheck:
      test:
        - CMD-SHELL
        - >-
          mysql -h127.0.0.1 -uroot -p"$$MYSQL_ROOT_PASSWORD" -N -s
          -e "SHOW GLOBAL STATUS LIKE 'Wesql_ready_for_write'" | grep -q 'ON$$'
      interval: 5s
      timeout: 5s
      retries: 12
      # Recovery from the object store can take minutes on a large snapshot.
      start_period: 300s
```

Dependent services should use `depends_on: {wesql: {condition: service_healthy}}`.

## Kubernetes

Use a startup probe with a long budget for recovery, a readiness probe on
`Wesql_ready_for_write`, and a liveness probe that only checks that mysqld
answers:

```yaml
startupProbe:
  exec:
    command: ["mysqladmin", "ping", "-h127.0.0.1", "--silent"]
  periodSeconds: 5
  failureThreshold: 120        # up to 10 minutes of recovery
readinessProbe:
  exec:
    command:
      - sh
      - -c
      - >-
        mysql -h127.0.0.1 -uroot -p"$MYSQL_ROOT_PASSWORD" -N -s
        -e "SHOW GLOBAL STATUS LIKE 'Wesql_ready_for_write'" | grep -q 'ON$'
  periodSeconds: 5
  failureThreshold: 3
livenessProbe:
  exec:
    command: ["mysqladmin", "ping", "-h127.0.0.1", "--silent"]
  periodSeconds: 10
  failureThreshold: 6
```

Use a dedicated probe account with only the `USAGE` privilege instead of
root where possible. `SHOW GLOBAL STATUS` needs no extra privilege.
