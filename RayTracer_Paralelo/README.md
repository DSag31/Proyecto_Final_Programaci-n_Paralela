# Ray Tracer paralelo (OpenMP) — Fase 3

Misma escena, mismo algoritmo y misma imagen que la versión serial; lo único que cambia es que las filas de la imagen se reparten entre varios hilos con OpenMP.

## Ejecutar (en la consola MSYS2 UCRT64)

Abrir **MSYS2 UCRT64** desde el menú de inicio (el ícono azul; no sirve MSYS2 MSYS ni MINGW64).

### 1. Entrar a la carpeta

En MSYS2 las unidades de Windows se escriben como `/e/`, `/c/`, etc. Si el proyecto está en `E:\Proyecto_Final_Programaci-n_Paralela`:

```bash
cd /e/Proyecto_Final_Programaci-n_Paralela/RayTracer_Paralelo
```

### 2. Comprobar que gcc está disponible

```bash
gcc --version
```

Si dice `command not found`, instalarlo una sola vez:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

### 3. Compilar (una sola vez)

```bash
gcc -O2 -fopenmp raytracer_paralelo.c -o paralelo.exe -lm
```

Mismas opciones que el serial (`-O2 -fopenmp`). Si funciona, no imprime nada y crea `paralelo.exe`.

### 4. Ejecutar con la semilla de referencia

```bash
./paralelo.exe 2073120322
```

Usa todos los hilos lógicos, `schedule(dynamic, 1)` y reparto por filas. Crea `raytracer_paralelo_seed_2073120322.ppm`.

### 5. Elegir hilos, schedule, chunk y modo

```bash
./paralelo.exe [semilla] [hilos] [schedule] [chunk] [modo] [guardar]
```

| Argumento | Valores | Por defecto |
|---|---|---|
| `semilla` | entero (sin número, sale de la hora) | hora actual |
| `hilos` | 1, 2, 4, 8, ... | núcleos lógicos |
| `schedule` | `static`, `dynamic`, `guided` | `dynamic` |
| `chunk` | entero; `0` = valor por defecto de OpenMP | `1` |
| `modo` | `filas` (parallel for sobre y) o `collapse` (collapse(2) sobre y, x) | `filas` |
| `guardar` | `si` o `no` (no escribe el .ppm) | `si` |

Ejemplos:

```bash
./paralelo.exe 2073120322 8 static 0 filas      # static, bloques seguidos
./paralelo.exe 2073120322 8 static 1 filas      # static,1: filas alternadas
./paralelo.exe 2073120322 8 dynamic 1 filas     # dynamic, 1 fila por turno
./paralelo.exe 2073120322 8 guided 1 filas      # guided
./paralelo.exe 2073120322 8 dynamic 64 collapse # collapse(2), bloques de 64 píxeles
./paralelo.exe 2073120322 1 static 0 filas      # 1 hilo (base para el speedup)
```

La salida muestra el tiempo del cálculo (sin guardar el archivo), cuántos píxeles hizo cada hilo, el **desbalance** (hilo más lento ÷ promedio; 1.00 es reparto perfecto) y una suma de verificación de la imagen.

## Verificar que la imagen es idéntica a la serial

Compilar y ejecutar el serial con la misma semilla, luego comparar byte a byte:

```bash
gcc -O2 -fopenmp ../RayTracer_Serial/raytracer_serial.c -o serial.exe -lm
./serial.exe 2073120322
./paralelo.exe 2073120322 8 dynamic 1 filas
cmp raytracer_seed_2073120322.ppm raytracer_paralelo_seed_2073120322.ppm && echo "IDENTICAS"
```

Si `cmp` no imprime nada y aparece `IDENTICAS`, la versión paralela es correcta.

## Mediciones automáticas (Fase 3 y 4)

```bash
bash benchmark_fase3.sh
```

Compila ambas versiones, verifica la imagen de cada configuración, mide el serial y el paralelo (hilos 1, 2, 4, 8; 5 corridas cada uno) y calcula **mediana, speedup, eficiencia y Karp-Flatt**. Deja los resultados en `Resultados_Fase3/resumen.csv` y `Resultados_Fase3/resultados_crudos.csv`. Dura unos 10 a 15 minutos con la configuración completa.

Para una prueba rápida o para cambiar lo que se mide:

```bash
REPS=1 THREADS="1 4" bash benchmark_fase3.sh
CONFIGS="static,0,filas dynamic,1,filas guided,1,filas" bash benchmark_fase3.sh
```

Para medir bien: conectar el equipo a la corriente, cerrar otros programas y no usar el computador mientras corre.

## Otra resolución o más muestras (opcional)

Se cambian al compilar. Ojo: el serial debe compilarse con los mismos valores para comparar.

```bash
gcc -O2 -fopenmp -DWIDTH=1920 -DHEIGHT=1080 -DSAMPLES=16 raytracer_paralelo.c -o paralelo_1080.exe -lm
```

## Qué cambió respecto al serial

- `#pragma omp parallel` con `#pragma omp for schedule(runtime)` sobre las filas (o `collapse(2)` sobre filas y columnas).
- El `schedule` y el `chunk` se eligen al ejecutar (`omp_set_schedule`), así no hay que recompilar para cada prueba.
- Se mide con `omp_get_wtime()` solo el cálculo, y con `omp_get_thread_num()` se registra el trabajo de cada hilo.
- `render_pixel`, la escena y el generador aleatorio no se tocaron: por eso la imagen es idéntica.
