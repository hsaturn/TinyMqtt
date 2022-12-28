SUB=

tests:
	set -e; \
	for i in ${SUB}*-tests/Makefile; do \
		echo '==== Making:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) -j; \
	done

debugtest:
	set -e; \
	$(MAKE) clean; \
	$(MAKE) -C debug-mode -j; \
	debug-mode/debug-tests.out

runtests: debugtest
	$(MAKE) clean
	$(MAKE) tests
	set -e; \
	for i in ${SUB}*-tests/Makefile; do \
		echo '==== Running:' $$(dirname $$i); \
		$$(dirname $$i)/$$(dirname $$i).out; \
	done

clean:
	set -e; \
	for i in ${SUB}*-tests/Makefile; do \
		echo '==== Cleaning:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) clean; \
	done
