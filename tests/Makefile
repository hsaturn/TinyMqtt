tests:
	set -e; \
	for i in *-tests/Makefile; do \
		echo '==== Making:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) -j; \
	done

runtests: tests
	set -e; \
	for i in *-tests/Makefile; do \
		echo '==== Running:' $$(dirname $$i); \
		$$(dirname $$i)/$$(dirname $$i).out; \
	done

clean:
	set -e; \
	for i in *-tests/Makefile; do \
		echo '==== Cleaning:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) clean; \
	done
