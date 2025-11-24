# PQTR

PQTR is a scalable media technology platform that streamlines enterprise creative workflows, from capture through collaboration to delivery.

## High-Level Architecture: MAINs

The repository is organized into top-level projects called **MAINs**. These are self-contained applications or services with their own executables. MAINs do not share code directly but may consume artifacts (like libraries) produced by other MAINs.

The current MAINs are:

*   [**`PQTR:LABS`**](./LABS/README.md): A digital film development laboratory, providing command-line tools and libraries for RAW image processing.
*   [**`PQTR:DESK`**](./DESK/README.md): A desktop application for viewing and optimizing image processing pipelines, acting as a GUI for `LABS`.
*   [**`PQTR:SITE`**](./SITE/README.md): The public-facing website for pqtr.ai.
*   [**`PQTR:FAST`**](./FAST/README.md): A mobile application for Android that utilizes `LABS` components.
