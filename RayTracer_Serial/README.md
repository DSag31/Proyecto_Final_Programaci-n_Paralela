# Ray Tracer serial

## Ejecutar (en PowerShell)

### 1. Entrar a la carpeta

```powershell
cd E:\Proyecto_Final_Programaci-n_Paralela\RayTracer_Serial
```

### 2. Poner `gcc` de MSYS2 en el PATH

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
```

Repetir este paso en cada ventana nueva de PowerShell.

### 3. Compilar (una sola vez)

```powershell
gcc -O2 -fopenmp raytracer_serial.c -o serial.exe -lm
```

Se compila con `-fopenmp` porque el programa usa `omp_get_wtime()`. Si funciona, no imprime nada y crea `serial.exe`.

### 4. Ejecutar con la semilla de referencia

```powershell
.\serial.exe 2073120322
```

Se crea `raytracer_seed_2073120322.ppm` en la misma carpeta.

### 5. Generar otras escenas (opcional)

Con otro número:

```powershell
.\serial.exe 1272306444
```

Sin número (la semilla sale de la hora y la escena cambia cada vez):

```powershell
.\serial.exe
```

Cada ejecución crea su propio archivo, con la semilla en el nombre.

## Semilla

El número que se pasa al ejecutar (`2073120322`) es la **semilla**: decide cómo queda la escena (esferas y luz).

- Misma semilla = misma escena.
- Otra semilla = otra escena.

Para comparar el serial con el paralelo, **ambos deben usar la misma semilla: `2073120322`**.
