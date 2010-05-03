all:
	cd priv; ${MAKE}
	cd src; ${MAKE}

clean:
	cd priv; ${MAKE} clean
	cd src; ${MAKE} clean
