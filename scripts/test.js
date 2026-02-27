class NodeTestClass {
    constructor() {
        this.val = 42;
    }
    doSomething(a) {
        return this.val + a;
    }
}

function node_test_function(left, right) {
    return left + right;
}

module.exports = {
    NodeTestClass,
    node_test_function
};
