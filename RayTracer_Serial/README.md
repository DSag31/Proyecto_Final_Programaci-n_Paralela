# Ray Tracer serial

## Ejecutar (en PowerShell)

```powershell
cd E:\Proyecto_Final_Programaci-n_Paralela\RayTracer_Serial
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
gcc -O2 -fopenmp raytracer_serial.c -o serial.exe -lm
.\serial.exe 2073120322
```

- La línea 2 pone `gcc` de MSYS2 al principio del PATH. Repetirla en cada ventana nueva de PowerShell.
- Se compila con `-fopenmp` porque el programa usa `omp_get_wtime()`.
- Se crea `raytracer_seed_2073120322.ppm` en la misma carpeta.

## Semilla

El número que se pasa al ejecutar (`2073120322`) es la **semilla**: decide cómo queda la escena (esferas y luz).

- Misma semilla = misma escena.
- Sin número, la semilla sale de la hora y la escena cambia cada vez.

Para comparar el serial con el paralelo, **ambos deben usar la misma semilla: `2073120322`**.
