"""
DiscussU Phase 3 Automated Tests

Run:
    python3 tests/protocol_tests.py localhost 6543 --clients 10 --requests 20

Start the server separately before running this script.
Run only against your own authorized server.

These tests create questions and answers in server memory.
They test the server through Python TCP clients, not the C++ menu.

Expected server format:
    Successful VIEW responses begin with 200 OK and end with END.
    Question details use labels such as Title: and Number of Answers:.
"""

import argparse
import getpass
import re
import select
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor


# ============================================================
# Command-Line Arguments and Test Credentials
# ============================================================

parser = argparse.ArgumentParser(description="DiscussU automated tests")
parser.add_argument("host", nargs="?", default="localhost")
parser.add_argument("port", nargs="?", type=int, default=6543)
parser.add_argument("--clients", type=int, default=10)
parser.add_argument("--requests", type=int, default=20)
args = parser.parse_args()

if not 1 <= args.port <= 65535:
    parser.error("Port must be between 1 and 65535.")

if args.clients < 3 or args.requests < 1:
    parser.error("Use at least 3 clients and 1 request per client.")

username = input("Configured test username: ")
password = getpass.getpass("Configured test password: ")

if not username or not password:
    parser.error("Username and password cannot be empty.")

for value in (username, password):
    if not value.isascii() or any(c in value for c in "|\n\r"):
        parser.error("Credentials must be ASCII without |, newline, or carriage return.")


# ============================================================
# Test TCP Client and Newline Framing
# ============================================================

class Client:
    """Maintains an independent TCP connection and receive buffer."""

    def __init__(self):
        self.socket = socket.create_connection(
            (args.host, args.port), timeout=10
        )
        self.socket.settimeout(10)
        self.buffer = b""

    def read_line(self):
        """Read one complete line while preserving unused received bytes."""

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

        if b"\r" in line:
            raise RuntimeError("Unexpected carriage return in response")

        return line.decode("ascii")

    def response(self, multiline=False):
        """Read a status line and, when applicable, data through END."""

        lines = [self.read_line()]

        if multiline and lines[0] == "200 OK":
            while True:
                line = self.read_line()
                lines.append(line)

                if line == "END":
                    break

        return lines

    def request(self, command, multiline=False):
        """Send one newline-delimited command and receive its response."""

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
        """Verify that QUIT is confirmed and then closes the connection."""

        assert self.request("QUIT") == ["200 OK"]
        assert not self.buffer, "Unexpected buffered data after QUIT"

        assert self.socket.recv(1) == b"", (
            "Server did not close the connection after QUIT"
        )

    def close(self):
        self.socket.close()


# ============================================================
# Common Test Helpers
# ============================================================

def expect_code(result, code):
    assert result[0].startswith(f"{code} "), (
        f"Expected response code {code}, received {result}"
    )


def expect_view(result):
    assert result[0] == "200 OK", result
    assert result[-1] == "END", "Missing END response marker"


def create_question(client, title):
    result = client.request(
        f"POST_QUESTION|title={title}|body=Automated Phase 3 test"
    )

    match = re.fullmatch(
        r"200 OK\|questionNumber=([1-9][0-9]*)", result[0]
    )

    assert match, f"Unexpected question response: {result}"
    return int(match.group(1))


def parse_answer_result(result, question_id):
    match = re.fullmatch(
        r"200 OK\|answerId=([1-9][0-9]*)"
        r"\|questionNumber=([1-9][0-9]*)",
        result[0]
    )

    assert match, f"Unexpected answer response: {result}"
    assert int(match.group(2)) == question_id
    return int(match.group(1))


# ============================================================
# Session-State Tests
# ============================================================

def test_session_states():
    client = Client()

    try:
        # Board commands before login must be rejected.
        result = client.request("VIEW_QUESTIONS")
        expect_code(result, 401)
        print(f"  Before login: {result[0]}")

        # Valid login must still work on the same connection.
        client.login()
        print("  Valid login succeeded after the rejected request")

        # A second login must not replace an authenticated session.
        result = client.request(
            f"LOGIN|username={username}|password={password}"
        )
        expect_code(result, 409)
        print(f"  Second login: {result[0]}")

        expect_view(client.request("VIEW_QUESTIONS", multiline=True))

        # LOGOUT keeps TCP open but clears authentication.
        assert client.request("LOGOUT") == ["200 OK"]

        result = client.request("VIEW_QUESTIONS")
        expect_code(result, 401)
        print(f"  After logout: {result[0]}")

        # LOGOUT without authentication must also be rejected.
        expect_code(client.request("LOGOUT"), 401)

        client.login()
        client.quit()
        print("  Login again and graceful QUIT succeeded")
    finally:
        client.close()


# ============================================================
# Invalid Requests and Recovery
# ============================================================

def test_invalid_requests():
    client = Client()

    try:
        # Ensure the submitted password differs from the configured one.
        wrong_password = "x" if password != "x" else "y"

        result = client.request(
            f"LOGIN|username={username}|password={wrong_password}"
        )
        expect_code(result, 410)
        print(f"  Wrong password: {result[0]}")

        client.login()
        question_id = create_question(
            client, "Invalid request recovery test"
        )

        cases = [
            ("Empty request", "", 300),
            ("Unknown command", "UNKNOWN_COMMAND", 400),
            (
                "Missing body",
                "POST_QUESTION|title=Missing body",
                300
            ),
            (
                "Empty title",
                "POST_QUESTION|title=|body=Test body",
                300
            ),
            (
                "Duplicate field",
                "POST_QUESTION|title=First|title=Second|body=Test",
                300
            ),
            (
                "Unexpected field",
                "VIEW_QUESTIONS|extra=value",
                300
            ),
            (
                "Nonnumeric question number",
                "VIEW_ANSWER|questionNumber=abc",
                300
            ),
            (
                "Zero question number",
                "VIEW_ANSWER|questionNumber=0",
                300
            ),
        ]

        for label, command, code in cases:
            result = client.request(command)
            expect_code(result, code)

            # A valid request must work after every error.
            recovery = client.request(
                f"VIEW_ANSWER|questionNumber={question_id}",
                multiline=True
            )
            expect_view(recovery)

            print(f"  {label}: {result[0]}; recovery succeeded")

        # Find a question number not currently in the board.
        summaries = client.request("VIEW_QUESTIONS", multiline=True)
        expect_view(summaries)

        existing_ids = [
            int(line.split(": ", 1)[1])
            for line in summaries
            if line.startswith("Question Number: ")
        ]

        assert existing_ids, "Question summaries omitted question numbers"
        missing_id = max(existing_ids) + 1000

        result = client.request(
            f"VIEW_ANSWER|questionNumber={missing_id}"
        )
        expect_code(result, 404)
        print(f"  Missing question: {result[0]}")

        expect_view(client.request("VIEW_QUESTIONS", multiline=True))
        client.quit()
    finally:
        client.close()


# ============================================================
# Fragmented Request Test
# ============================================================

def test_fragmented_request():
    client = Client()

    try:
        client.login()
        assert not client.buffer, "Unexpected unread data after login"

        # First write deliberately does not include a newline.
        client.socket.sendall(b"VIEW_QUES")
        print("  Sent fragment 1: VIEW_QUES (without newline)")

        # This interval checks that incomplete input is not processed.
        # The application's framing must not depend on this timing.
        readable, _, _ = select.select(
            [client.socket], [], [], 0.3
        )

        assert not readable, (
            "Server responded before the terminating newline"
        )
        print("  No response before the newline")

        # Complete the same logical request.
        client.socket.sendall(b"TIONS\n")
        print("  Sent fragment 2: TIONS\\n")

        result = client.response(multiline=True)
        expect_view(result)
        print("  Received one complete response ending with END")

        # Verify that later commands remain usable.
        expect_view(client.request("VIEW_QUESTIONS", multiline=True))
        client.quit()
    finally:
        client.close()


# ============================================================
# Batched Request Test
# ============================================================

def test_batched_requests():
    client = Client()

    try:
        client.login()

        # Place two logical requests in one supplied byte string.
        client.socket.sendall(
            b"VIEW_QUESTIONS\nVIEW_QUESTIONS\n"
        )
        print("  Sent two VIEW_QUESTIONS requests together")

        first = client.response(multiline=True)
        second = client.response(multiline=True)

        expect_view(first)
        expect_view(second)

        # No other test workers are running during this test.
        assert first == second, (
            "Unchanged-board responses differed"
        )

        print("  Received two complete responses ending with END")
        client.quit()
    finally:
        client.close()


# ============================================================
# Three Connected Clients Performing Different Operations
# ============================================================

def test_three_clients_different_operations():
    clients = []

    try:
        for _ in range(3):
            client = Client()
            clients.append(client)
            client.login()

        poster, viewer, answerer = clients
        print("  Three clients are connected and authenticated")

        question_id = create_question(
            poster, "Three-client shared board test"
        )
        print(f"  Client 1 created question {question_id}")

        result = viewer.request(
            f"VIEW_ANSWER|questionNumber={question_id}",
            multiline=True
        )
        expect_view(result)

        assert "Title: Three-client shared board test" in result
        assert "Number of Answers: 0" in result
        print("  Client 2 retrieved Client 1's question")

        answer_text = "Answer from the third connected client"
        result = answerer.request(
            f"POST_ANSWER|questionNumber={question_id}"
            f"|answer={answer_text}"
        )

        answer_id = parse_answer_result(result, question_id)
        print(f"  Client 3 posted answer {answer_id}")

        result = poster.request(
            f"VIEW_ANSWER|questionNumber={question_id}",
            multiline=True
        )
        expect_view(result)

        assert "Number of Answers: 1" in result
        assert f"Answer: {answer_text}" in result
        print("  Client 1 retrieved Client 3's answer")

        for client in clients:
            client.quit()
    finally:
        for client in clients:
            client.close()


# ============================================================
# Concurrent Answers to the Same Question
# ============================================================

def test_concurrent_answers():
    owner = Client()

    try:
        owner.login()
        question_id = create_question(owner, "Concurrent answer test")
        barrier = threading.Barrier(3)

        def worker(number):
            client = None

            try:
                client = Client()
                client.login()

                # All workers must be logged in before submitting answers.
                barrier.wait(timeout=10)

                answer_text = f"Concurrent answer from worker {number}"
                result = client.request(
                    f"POST_ANSWER|questionNumber={question_id}"
                    f"|answer={answer_text}"
                )

                answer_id = parse_answer_result(result, question_id)

                client.quit()
                return answer_id, answer_text
            except Exception:
                barrier.abort()
                raise
            finally:
                if client is not None:
                    client.close()

        with ThreadPoolExecutor(max_workers=3) as pool:
            results = list(pool.map(worker, range(1, 4)))

        answer_ids = [result[0] for result in results]
        assert len(set(answer_ids)) == 3, "Duplicate answer IDs"

        discussion = owner.request(
            f"VIEW_ANSWER|questionNumber={question_id}",
            multiline=True
        )
        expect_view(discussion)

        assert "Number of Answers: 3" in discussion

        for _, answer_text in results:
            assert f"Answer: {answer_text}" in discussion, (
                f"Lost answer: {answer_text}"
            )

        print(f"  Stored 3 answers with unique IDs: {answer_ids}")
        owner.quit()
    finally:
        owner.close()


# ============================================================
# Invalid Request and Disconnect Isolation
# ============================================================

def test_failure_isolation():
    clients = []

    try:
        for _ in range(3):
            client = Client()
            clients.append(client)
            client.login()

        bad_client, survivor1, survivor2 = clients

        result = bad_client.request(
            "POST_QUESTION|title=Missing body"
        )
        assert result == ["300 message format error"], result

        for survivor in (survivor1, survivor2):
            expect_view(
                survivor.request("VIEW_QUESTIONS", multiline=True)
            )

        expect_view(
            bad_client.request("VIEW_QUESTIONS", multiline=True)
        )
        print("  Invalid request rejected; all connections remain usable")

        # Close without QUIT to simulate an unexpected client termination.
        bad_client.close()
        print("  One client closed without QUIT")

        id1 = create_question(survivor1, "Survivor one")
        id2 = create_question(survivor2, "Survivor two")
        assert id1 != id2

        for question_id in (id1, id2):
            expect_view(
                survivor1.request(
                    f"VIEW_ANSWER|questionNumber={question_id}",
                    multiline=True
                )
            )

        print("  Remaining clients posted and retrieved both questions")

        survivor1.quit()
        survivor2.quit()
    finally:
        for client in clients:
            client.close()


# ============================================================
# Repeated Connections and Graceful QUIT
# ============================================================

def test_repeated_connections():
    for _ in range(15):
        client = Client()

        try:
            client.login()
            expect_view(
                client.request("VIEW_QUESTIONS", multiline=True)
            )
            client.quit()
        finally:
            client.close()

    print("  Completed 15 connect/login/view/QUIT cycles")


# ============================================================
# Concurrent Load Test
# ============================================================

def run_load_test():
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
            # Include requests not completed after a connection/setup error.
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
    failed_requests = sum(result[1] for result in results)
    durations = [value for result in results for value in result[2]]
    ids = [qid for result in results for qid in result[3]]
    worker_errors = [result[4] for result in results if result[4]]

    print("\nLOAD TEST RESULTS")
    print(f"Clients: {args.clients}")
    print(f"Planned posting requests: {args.clients * args.requests}")
    print(f"Successful posting requests: {successful}")
    print(f"Failed/not completed posting requests: {failed_requests}")
    print(f"Total duration including connection/cleanup: {elapsed:.3f} seconds")

    if durations:
        average_ms = sum(durations) / len(durations) * 1000
        maximum_ms = max(durations) * 1000

        print(f"Average completed-request response time: {average_ms:.3f} ms")
        print(f"Maximum completed-request response time: {maximum_ms:.3f} ms")

    for index, result in enumerate(results):
        if result[4]:
            print(f"Worker {index} error: {result[4]}")

    assert not worker_errors, f"Worker failures: {worker_errors}"
    assert failed_requests == 0, (
        f"{failed_requests} posting requests failed or were not completed"
    )
    assert len(ids) == len(set(ids)), "Duplicate question IDs under load"

    # Verification is outside the timed posting workload.
    checker = Client()

    try:
        checker.login()

        for question_id in ids:
            result = checker.request(
                f"VIEW_ANSWER|questionNumber={question_id}",
                multiline=True
            )
            expect_view(result)

            assert "Question: Load test question" in result, (
                f"Question {question_id} was not returned correctly"
            )

        checker.quit()
    finally:
        checker.close()

    print(f"Verified that all {len(ids)} created questions were retained.")
    print("Verification requests are additional to the timed posting workload.")


# ============================================================
# Run Tests and Report Actual Results
# ============================================================

tests = [
    ("Session-state validation", test_session_states),
    ("Invalid requests and recovery", test_invalid_requests),
    ("Fragmented request", test_fragmented_request),
    ("Batched requests", test_batched_requests),
    (
        "Three connected clients perform different operations",
        test_three_clients_different_operations
    ),
    (
        "Concurrent answers retained with unique IDs",
        test_concurrent_answers
    ),
    (
        "Invalid request and disconnect isolation",
        test_failure_isolation
    ),
    (
        "Repeated connections and graceful QUIT",
        test_repeated_connections
    ),
    ("Concurrent load test", run_load_test),
]

passed = 0
failed = 0

for name, function in tests:
    print(f"\nTEST: {name}")

    try:
        function()
        passed += 1
        print(f"PASS: {name}")
    except Exception as error:
        failed += 1
        print(f"FAIL: {name}: {type(error).__name__}: {error}")

print("\nFINAL TEST SUMMARY")
print(f"Total test groups: {len(tests)}")
print(f"Passed: {passed}")
print(f"Failed: {failed}")

raise SystemExit(1 if failed else 0)