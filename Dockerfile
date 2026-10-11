# Use a lightweight Alpine Linux image with g++ installed
FROM alpine:3.18

# Install GNU C++ compiler and make
RUN apk add --no-cache g++ bash

# Set the working directory
WORKDIR /app

# Copy the repository into the container
COPY . /app/

# Make the test script executable
RUN chmod +x /app/tests/build_tests.sh

# Change to the tests directory and run the test script as the container entrypoint
WORKDIR /app/tests
CMD ["./build_tests.sh"]
