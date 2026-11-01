# EdyMastericer

Plugin de masterizacion estereo (VST3) para Windows, original e inspirado en el tipo de herramientas de Ozone 4.

Incluye: ecualizador de 3 bandas, compresor multibanda (graves / medios / agudos),
ampliador estereo por bandas, exciter armonico, maximizador con limitador y medidores.

## Como obtener el instalador (.exe) sin saber programar

Solo necesitas un navegador y una cuenta gratis de GitHub.

1. Entra a https://github.com y crea una cuenta (o inicia sesion).
2. Arriba a la derecha pulsa el boton "+" y elige "New repository".
   - Nombre: `EdyMastericer`
   - Marca "Public" (los repositorios publicos compilan gratis y sin limite).
   - Pulsa "Create repository".
3. En la pagina del repositorio nuevo pulsa el enlace "uploading an existing file".
4. Abre en tu computador la carpeta de este proyecto y **arrastra todo su contenido**
   (las carpetas `.github` y `Source`, y los archivos `CMakeLists.txt` e `installer.iss`)
   hacia la ventana del navegador. Importante: la carpeta `.github` debe subirse completa.
   Si tu Windows no la muestra, activa "Elementos ocultos" en la pestana Vista del Explorador de archivos.
5. Abajo pulsa "Commit changes".
6. Pulsa la pestana **Actions**. Veras una ejecucion llamada "Compilar EdyMastericer" que empieza sola.
   Tarda entre 10 y 20 minutos la primera vez. Cuando aparezca un circulo verde con una marca, termino.
   (Si en algun paso aparece un boton verde para habilitar los workflows, pulsalo.)
7. Entra a esa ejecucion y baja hasta la seccion **Artifacts**. Descarga `EdyMastericer_Setup`.
   Es un .zip: abrelo y dentro esta `EdyMastericer_Setup.exe`.
8. Doble clic en `EdyMastericer_Setup.exe` y sigue el asistente.

Si el circulo sale rojo, entra a la ejecucion, abre el paso que fallo y copia el mensaje de error: con eso se corrige.

## Usarlo en Cubase 5

1. Abre Cubase y ve a Dispositivos > Plugin Information (o reinicia Cubase para que re-escanee).
2. Abre una pista y, en la mezcladora, pon **EdyMastericer** como efecto de insercion en el canal Master (salida estereo).
   Aparece en la categoria Fx o Mastering.

El instalador pone la version de 32 bits en `C:\Program Files (x86)\Common Files\VST3`
y la de 64 bits en `C:\Program Files\Common Files\VST3`. Cubase usa la que le corresponde.

## Controles

- ECUALIZADOR: graves (shelf 120 Hz), medios (1 kHz), agudos (shelf 6 kHz), de -12 a +12 dB.
- COMPRESOR MULTIBANDA: umbral y ratio, aplicados a las tres bandas (cortes en 200 Hz y 3 kHz).
- AMPLIADOR ESTEREO: ancho por banda. 100 % = original, 0 % = mono, 200 % = el doble de ancho.
- EXCITER: suma brillo y armonicos a los agudos.
- MAXIMIZER / LIMITADOR: Drive sube el nivel hacia el limitador, Techo es el nivel maximo de salida, Release es la velocidad de recuperacion.
- ENTRADA / SALIDA: ganancia antes y despues de todo el proceso.
- BYPASS: apaga todo el efecto.

## Licencia

Este proyecto usa el framework JUCE (licencia AGPLv3 o comercial). Para uso personal no hay problema.
Si algun dia lo distribuyes a otras personas, tendras que publicar tambien el codigo fuente (o comprar una licencia de JUCE).
