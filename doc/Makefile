# ------------------------------------------------------------------------------
# Makefile for documentation build
# SPDX-License-Identifier: Apache-2.0

BUILDDIR ?= _build
DOC_TAG ?= development
SPHINXOPTS ?= -j auto
LATEXMKOPTS ?= -halt-on-error -no-shell-escape

# ------------------------------------------------------------------------------
# Documentation targets

.PHONY: configure clean html html-fast latex pdf doxygen

html-fast:
	${MAKE} html

html latex pdf doxygen: configure
	cmake --build ${BUILDDIR} --target $@

configure:
	cmake \
		-GNinja \
		-B${BUILDDIR} \
		-S. \
		-DDOC_TAG=${DOC_TAG} \
		-DSPHINXOPTS="${SPHINXOPTS}" \
		-DLATEXMKOPTS="${LATEXMKOPTS}"

clean:
	cmake --build ${BUILDDIR} --target clean
