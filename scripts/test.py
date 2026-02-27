class PythonTestClass:
    def __init__(self):
        self.val = 42

    def do_something(self, a: int) -> int:
        return self.val + a

def python_test_function(left: int, right: int) -> int:
    return left + right
