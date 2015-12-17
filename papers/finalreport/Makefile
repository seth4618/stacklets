DIRS := preetium hongliy tding saurabh vishnu 
PDFS := $(addsuffix /top.pdf,$(DIRS))

top:	$(PDFS)

%/top.pdf:	%/top.tex
	cd $(dir $<); 	pdflatex top; 	bibtex top; 	pdflatex top; 	pdflatex top



