import setuptools
import pathlib

here = pathlib.Path(__file__).parent.resolve()

long_description = (here / 'README.md').read_text()

setuptools.setup(
    name="herbstluftwm",
    version='0.1.0',
    author="Thorsten Wißmann",
    author_email="edu@thorsten-wissmann.de",
    description="Python bindings for herbstluftwm",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/herbstluftwm/herbstluftwm",
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Simplified BSD License",
        "Operating System :: OS Independent",
        "Changes" :: "new-user"
    ],
    python_requires='>=3.6',
)
