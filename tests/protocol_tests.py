import argparse
import getpass
import re
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor

parser = argparse.ArgumentParser()
parser.add_argument("host", nargs="?", default="localhost")
parser.add_argument("port", nargs="?", type=int, default=6543)
parser.add_argument("--clients", type=int, default=10)
parser.add_argument("--requests", type=int, default=20)
args = parser.parse_args()

if args.clients < 3 or args.requests < 1:
    parser.error("Use at least 3 clients and at least 1 request per client.")

username = input("Configured test username: ")
password = getpass.getpass("Configured test password: ")


class Client:
    """Test connection with persistent newline buffering."""

    def __init__(self):
        self.socket = socket.create_connection(
            (args.host, args.port), timeout=10
        )
        self.socket.settimeout(10)
        self.buffer = b""

    def read_line(self):
        while b"\n" not in self.buffer:
            data = self.socket.recv(1024)

            if not data:
                raise RuntimeError("Server closed the connection")

            self.buffer += data

            if b"\n" not in self.buffer and len(self.buffer) >= 4096:
                raise RuntimeError("Oversized response line")

        line, self.buffer = self.buffer.split(b"\n", 1)

        if len(line) + 1 > 4096:
            raise RuntimeError("Oversized response line")

        return line.decode("ascii")

    def response(self, multiline=False):
        lines = [self.read_line()]

        if multiline and lines[0] == "200 OK":
            while True:
                line = self.read_line()
                lines.append(line)

                if line == "END":
                    break

        return lines

    def request(self, command, multiline=False):
        self.socket.sendall((command + "\n").encode("ascii"))
        return self.response(multiline)

    def login(self):
        result = self.request(
            f"LOGIN|username={username}|password={password}"
        )

        assert re.fullmatch(
            r"200 OK\|userId=[1-9][0-9]*", result[0]
        ), f"Login failed: {result}"

    def quit(self):
        assert self.request("QUIT") == ["200 OK"]
        assert not self.buffer, "Unexpected data after QUIT"
        assert self.socket.recv(1) == b"", "Server did not close after QUIT"

    def close(self):
        self.socket.close()


def create_question(client, title):
    result = client.request(
        f"POST_QUESTION|title={title}|body=Automated Phase 3 test"
    )

    match = re.fullmatch(
        r"200 OK\|questionNumber=([1-9][0-9]*)", result[0]
    )
    assert match, f"Unexpected question response: {result}"
    return int(match.group(1))


def test_concurrent_answers():
    """Three logged-in clients answer the same question together."""

    owner = Client()

    try:
        owner.login()
        question_id = create_question(owner, "Concurrent answer test")
        barrier = threading.Barrier(3)

        def worker(number):
            client = Client()

            try:
                client.login()

                # All three clients must be logged in before posting.
                barrier.wait(timeout=10)

                answer_text = f"Concurrent answer from worker {number}"
                result = client.request(
                    f"POST_ANSWER|questionNumber={question_id}"
                    f"|answer={answer_text}"
                )

                match = re.fullmatch(
                    r"200 OK\|answerId=([1-9][0-9]*)"
                    r"\|questionNumber=([1-9][0-9]*)",
                    result[0]
                )

                assert match, f"Unexpected answer response: {result}"
                assert int(match.group(2)) == question_id

                client.quit()
                return int(match.group(1)), answer_text
            finally:
                client.close()

        with ThreadPoolExecutor(max_workers=3) as pool:
            results = list(pool.map(worker, range(1, 4)))

        answer_ids = [result[0] for result in results]
        assert len(set(answer_ids)) == 3, "Duplicate answer IDs"

        discussion = owner.request(
            f"VIEW_ANSWER|questionNumber={question_id}",
            multiline=True
        )

        assert discussion[0] == "200 OK"
        assert discussion[-1] == "END"
        assert "Number of Answers: 3" in discussion

        for _, answer_text in results:
            assert f"Answer: {answer_text}" in discussion, (
                f"Lost answer: {answer_text}"
            )

        print(f"  Stored 3 answers with unique IDs: {answer_ids}")
        owner.quit()
    finally:
        owner.close()


def test_failure_isolation():
    """An invalid request and a disconnected client do not stop others."""

    clients = []

    try:
        for _ in range(3):
            client = Client()
            clients.append(client)
            client.login()

        bad_client, survivor1, survivor2 = clients

        # A malformed request must be rejected.
        result = bad_client.request("POST_QUESTION|title=Missing body")
        assert result == ["300 message format error"], result

        # Other clients must still receive complete responses.
        for survivor in (survivor1, survivor2):
            result = survivor.request("VIEW_QUESTIONS", multiline=True)
            assert result[0] == "200 OK"
            assert result[-1] == "END"

        # The affected client should also recover after its invalid request.
        result = bad_client.request("VIEW_QUESTIONS", multiline=True)
        assert result[0] == "200 OK"

        # Close without QUIT to simulate unexpected termination.
        bad_client.close()

        # Remaining clients must still be able to modify shared data.
        id1 = create_question(survivor1, "Survivor one")
        id2 = create_question(survivor2, "Survivor two")
        assert id1 != id2

        # Verify both questions remain retrievable.
        for question_id in (id1, id2):
            result = survivor1.request(
                f"VIEW_ANSWER|questionNumber={question_id}",
                multiline=True
            )
            assert result[0] == "200 OK"
            assert result[-1] == "END"

        survivor1.quit()
        survivor2.quit()
    finally:
        for client in clients:
            client.close()


def test_repeated_connections():
    """Repeated login/QUIT cycles leave the server available."""

    for _ in range(15):
        client = Client()

        try:
            client.login()
            result = client.request("VIEW_QUESTIONS", multiline=True)
            assert result[0] == "200 OK"
            assert result[-1] == "END"
            client.quit()
        finally:
            client.close()


def run_load_test():
    """Concurrent valid posting requests with measured response times."""

    barrier = threading.Barrier(args.clients)
    run_tag = time.time_ns()
    started = time.perf_counter()

    def worker(number):
        successes = 0
        failures = 0
        durations = []
        question_ids = []
        client = None
        problem = None

        try:
            client = Client()
            client.login()
            barrier.wait(timeout=15)

            for request_number in range(args.requests):
                request_started = time.perf_counter()

                result = client.request(
                    f"POST_QUESTION|title=Load {run_tag}-{number}-{request_number}"
                    "|body=Load test question"
                )

                durations.append(
                    time.perf_counter() - request_started
                )

                match = re.fullmatch(
                    r"200 OK\|questionNumber=([1-9][0-9]*)",
                    result[0]
                )

                if match:
                    successes += 1
                    question_ids.append(int(match.group(1)))
                else:
                    failures += 1

            client.quit()
        except Exception as error:
            # Count requests not successfully completed as failures.
            failures = args.requests - successes
            problem = str(error)
            barrier.abort()
        finally:
            if client is not None:
                client.close()

        return successes, failures, durations, question_ids, problem

    with ThreadPoolExecutor(max_workers=args.clients) as pool:
        results = list(pool.map(worker, range(args.clients)))

    elapsed = time.perf_counter() - started
    successful = sum(result[0] for result in results)
    failed = sum(result[1] for result in results)
    durations = [t for result in results for t in result[2]]
    ids = [qid for result in results for qid in result[3]]

    print("\nLOAD TEST RESULTS")
    print(f"Clients: {args.clients}")
    print(f"Planned posting requests: {args.clients * args.requests}")
    print(f"Successful posting requests: {successful}")
    print(f"Failed/not completed posting requests: {failed}")
    print(f"Total duration including connection/cleanup: {elapsed:.3f} seconds")

    if durations:
        print(
            "Average completed-request response time: "
            f"{sum(durations) / len(durations) * 1000:.3f} ms"
        )
        print(f"Maximum completed-request response time: {max(durations) * 1000:.3f} ms")

    for index, result in enumerate(results):
        if result[4]:
            print(f"Worker {index} error: {result[4]}")

    assert failed == 0, f"{failed} requests failed or were not completed"
    assert len(ids) == len(set(ids)), "Duplicate question IDs under load"

    # Confirm every successfully created question is retrievable.
    checker = Client()

    try:
        checker.login()

        for question_id in ids:
            result = checker.request(
                f"VIEW_ANSWER|questionNumber={question_id}",
                multiline=True
            )
            assert result[0] == "200 OK", (
                f"Question {question_id} was not retained"
            )
            assert result[-1] == "END"

        checker.quit()
    finally:
        checker.close()

    print(f"Verified that all {len(ids)} created questions were retained.")
    print("Verification requests are additional to the timed posting workload.")


passed = 0
failed = 0

tests = [
    ("Concurrent answers retained with unique IDs", test_concurrent_answers),
    ("Invalid request and disconnect isolation", test_failure_isolation),
    ("Repeated connections and graceful QUIT", test_repeated_connections),
    ("Concurrent load test", run_load_test),
]

for name, function in tests:
    try:
        function()
        passed += 1
        print(f"PASS: {name}")
    except Exception as error:
        failed += 1
        print(f"FAIL: {name}: {error}")

print(f"\nTotal test groups: {len(tests)}")
print(f"Passed: {passed}")
print(f"Failed: {failed}")

raise SystemExit(1 if failed else 0)