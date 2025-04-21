.PHONY: build run clean local

build:
	@echo "Building judge-execute..."
	./scripts/build.sh

run:
	@echo "Running judge-execute..."
	./scripts/run.sh

clean:
	@echo "Cleaning judge-execute..."
	./scripts/clean.sh

local:
	@echo "Running judge-execute locally..."
	./scripts/local.sh