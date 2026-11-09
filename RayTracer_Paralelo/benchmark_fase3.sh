#!/usr/bin/env bash
# Benchmark de la Fase 3: compara serial vs paralelo (OpenMP) y arma el resumen.
# Ejecutar desde la consola MSYS2 UCRT64:   bash benchmark_fase3.sh
#
# Variables opcionales (se pueden cambiar sin editar el archivo):
#   SEED=2073120322     semilla de la escena
#   REPS=5              corridas por configuracion (se reporta la mediana)
#   THREADS="1 2 4 8"   cantidades de hilos a probar
#   CONFIGS="..."       lista "schedule,chunk,modo" separadas por espacios
# Ejemplo rapido:  REPS=1 THREADS="1 4" bash benchmark_fase3.sh

cd "$(dirname "$0")" || exit 1

SEED=${SEED:-2073120322}
REPS=${REPS:-5}
THREADS=${THREADS:-"1 2 4 8"}
CONFIGS=${CONFIGS:-"static,0,filas static,1,filas dynamic,1,filas dynamic,4,filas guided,1,filas static,0,collapse dynamic,64,collapse guided,16,collapse"}

OUT=Resultados_Fase3
mkdir -p "$OUT"
RAW="$OUT/resultados_crudos.csv"
SUM="$OUT/resumen.csv"

echo "== Compilando =="
gcc -O2 -fopenmp raytracer_paralelo.c -o paralelo.exe -lm || exit 1
gcc -O2 -fopenmp ../RayTracer_Serial/raytracer_serial.c -o serial.exe -lm || exit 1

echo "configuracion,hilos,corrida,tiempo,desbalance" > "$RAW"

# ---------- 1) Baseline serial ----------
echo "== Serial: $REPS corridas =="
for r in $(seq 1 "$REPS"); do
    t=$(./serial.exe "$SEED" | grep "Tiempo serial" | awk '{print $3}')
    echo "serial,1,$r,$t,1.0000" >> "$RAW"
    echo "  serial corrida $r: $t s"
done

# ---------- 2) Correctitud: imagen paralela == imagen serial ----------
echo "== Verificando que la imagen paralela sea identica a la serial =="
OK=1
for cfg in $CONFIGS; do
    IFS=, read -r sch chunk modo <<< "$cfg"
    ./paralelo.exe "$SEED" 8 "$sch" "$chunk" "$modo" si > /dev/null
    if cmp -s "raytracer_seed_$SEED.ppm" "raytracer_paralelo_seed_$SEED.ppm"; then
        echo "  OK    $sch chunk $chunk $modo"
    else
        echo "  FALLA $sch chunk $chunk $modo  (la imagen NO coincide)"
        OK=0
    fi
done
[ "$OK" = 1 ] || { echo "Hay diferencias: no se mide hasta corregirlas."; exit 1; }

# ---------- 3) Mediciones paralelas ----------
echo "== Paralelo =="
for cfg in $CONFIGS; do
    IFS=, read -r sch chunk modo <<< "$cfg"
    for p in $THREADS; do
        for r in $(seq 1 "$REPS"); do
            line=$(./paralelo.exe "$SEED" "$p" "$sch" "$chunk" "$modo" no | grep "^RESULTADO")
            t=$(echo "$line" | cut -d, -f10)
            d=$(echo "$line" | cut -d, -f11)
            echo "$sch-$chunk-$modo,$p,$r,$t,$d" >> "$RAW"
        done
        echo "  $sch chunk $chunk $modo, $p hilos: listo"
    done
done

# ---------- 4) Resumen: mediana, speedup, eficiencia, Karp-Flatt ----------
awk -F, '
function median(key,   n, i, j, tmp, arr) {
    n = cnt[key]
    for (i = 1; i <= n; i++) arr[i] = val[key, i]
    for (i = 2; i <= n; i++) {            # ordenar (insercion)
        tmp = arr[i]
        for (j = i - 1; j >= 1 && arr[j] > tmp; j--) arr[j+1] = arr[j]
        arr[j+1] = tmp
    }
    return (n % 2) ? arr[(n+1)/2] : (arr[n/2] + arr[n/2+1]) / 2
}
NR > 1 {
    key = $1 SUBSEP $2
    cnt[key]++; val[key, cnt[key]] = $4
    dcnt[key]++; dsum[key] += $5
    if (!(key in seen)) { seen[key] = 1; order[++m] = key }
}
END {
    ts = median("serial" SUBSEP 1)
    print "configuracion,hilos,mediana_s,speedup,eficiencia,karp_flatt,desbalance_prom"
    printf "serial,1,%.4f,1.0000,1.0000,,1.0000\n", ts
    for (k = 1; k <= m; k++) {
        key = order[k]
        split(key, parts, SUBSEP)
        if (parts[1] == "serial") continue
        p = parts[2]; tm = median(key); s = ts / tm; e = s / p
        kf = (p > 1) ? ((1/s - 1/p) / (1 - 1/p)) : ""
        if (kf != "") kf = sprintf("%.4f", kf)
        printf "%s,%d,%.4f,%.4f,%.4f,%s,%.4f\n", parts[1], p, tm, s, e, kf, dsum[key]/dcnt[key]
    }
}' "$RAW" > "$SUM"

echo
echo "== RESUMEN (mediana de $REPS corridas) =="
column -s, -t "$SUM" 2>/dev/null || cat "$SUM"
echo
echo "Archivos: $RAW  y  $SUM"
