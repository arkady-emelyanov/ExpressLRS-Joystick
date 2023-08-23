.PHONY: clean
clean:
	pio run -t clean

.PHONY: build
build:
	pio run -s -e xaio-samd21

.PHONY: check
check:
	pio check --skip-packages

.PHONY: upload
upload:
	pio run -t nobuild -t upload
