# HALI Research Presentation

This directory contains the LaTeX Beamer presentation for the HALI (Hierarchical Adaptive Learned Index) research project.

## File Structure

- **presentation.tex** - Main LaTeX Beamer presentation (Berkeley theme, Seahorse color scheme)
- **figures/** - Directory for external figures (currently unused, all diagrams are generated via TikZ)

## Building the Presentation

### Option 1: Using pdflatex (Recommended)

```bash
cd report
pdflatex presentation.tex
pdflatex presentation.tex  # Run twice for TOC and references
```

### Option 2: Using latexmk (Automated)

```bash
cd report
latexmk -pdf presentation.tex
```

### Option 3: Using Overleaf

Upload `presentation.tex` to [Overleaf](https://www.overleaf.com) for online compilation.

## Required LaTeX Packages

The presentation uses the following packages (all standard in TeX Live/MiKTeX):

- **beamer** - Presentation framework
- **tikz** - Vector graphics
- **pgfplots** - Data visualization
- **booktabs** - Professional tables
- **algorithm** & **algorithmic** - Algorithm typesetting
- **amsmath** & **amssymb** - Mathematical symbols
- **graphicx** - Graphics inclusion
- **listings** - Code syntax highlighting
- **xcolor** - Color support

## Presentation Structure

**Total:** 45+ slides covering the complete HALI research journey from HALIv1 through HALIv2.

### Main Sections

1. Introduction & Motivation
2. Background: Learned Indexes
3. HALIv1: Initial Design
4. HALIv1: Critical Issues
5. HALIv2: Architectural Improvements
6. HALIv2: Performance Results
7. Comparative Analysis
8. Lessons Learned
9. Future Work & Conclusion
10. Backup Slides

## Key Visualizations

All visualizations are generated inline using TikZ/PGFPlots:

- Pareto frontier plots
- Performance comparison charts
- Architecture diagrams
- Algorithm pseudocode
- Comprehensive performance tables

## Author Information

- **Author:** Dhruv Joshi
- **Email:** jdhruv1503@gmail.com
- **Institution:** Operating Systems Course Project
- **Date:** November 4, 2025

## Related Documentation

For detailed technical information, see:

- `../README.md` - Project overview
- `../documentation/PRODUCTION_RESULTS.md` - Full benchmark results
- `../documentation/HALIV2_IMPROVEMENTS.md` - Architectural improvements
- `../documentation/RESEARCH_PROGRESS.md` - Research log
