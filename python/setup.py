import setuptools
import pathlib
import os

here = pathlib.Path(__file__).parent.resolve()

with open(os.path.join(here, "README.md"), "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="herbstluftwm",
    version='0.1.0',
    author="Thorsten Wißmann",
    author_email="edu@thorsten-wissmann.de",
    description="python bindings for herbstluftwm",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/herbstluftwm/herbstluftwm",
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Simplified BSD License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.6',
)
