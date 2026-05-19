from __future__ import annotations
from enum import Enum, auto
from typing import Optional

_builtin_list = list


class Kind(Enum):
    NIL = auto()
    BOOL = auto()
    INT = auto()
    FLOAT = auto()
    STRING = auto()
    SYMBOL = auto()
    KEYWORD = auto()
    LIST = auto()


class Node:
    __slots__ = ('kind', 'bool_val', 'int_val', 'float_val', 'str_val', 'items')

    def __init__(self, kind: Kind = Kind.NIL, str_val: str = ''):
        self.kind = kind
        self.bool_val = False
        self.int_val = 0
        self.float_val = 0.0
        self.str_val = str_val
        self.items: list[Node] = []

    def push(self, child: Node) -> Node:
        if self.kind != Kind.LIST:
            prev = [self] if self.kind != Kind.NIL else []
            self.kind = Kind.LIST
            self.items = prev
        self.items.append(child)
        return self

    def kv(self, key: str, val: Node) -> Node:
        if self.kind != Kind.LIST:
            self.kind = Kind.LIST
            self.items = []
        self.items.append(keyword(key))
        self.items.append(val)
        return self

    def _needs_pipe_escape(self, s: str) -> bool:
        for ch in s:
            if ch in ' ()"\'\\|:\t\n\r' or ord(ch) < 32:
                return True
        return False

    def _escape_symbol(self, s: str) -> str:
        if not self._needs_pipe_escape(s):
            return s
        result = []
        for ch in s:
            if ch == '|':
                result.append('\\|')
            elif ch == '\\':
                result.append('\\\\')
            else:
                result.append(ch)
        return '|' + ''.join(result) + '|'

    def _escape_string(self, s: str) -> str:
        result = []
        for ch in s:
            if ch == '"':
                result.append('\\"')
            elif ch == '\\':
                result.append('\\\\')
            elif ch == '\n':
                result.append('\\n')
            elif ch == '\r':
                result.append('\\r')
            elif ch == '\t':
                result.append('\\t')
            elif ord(ch) < 32:
                result.append(f'\\x{ord(ch):02x}')
            else:
                result.append(ch)
        return ''.join(result)

    def to_string(self, pretty: bool = False, depth: int = 0) -> str:
        indent = '  ' * depth if pretty else ''
        inner_indent = '  ' * (depth + 1) if pretty else ''

        if self.kind == Kind.NIL:
            return 'nil'
        if self.kind == Kind.BOOL:
            return 'true' if self.bool_val else 'false'
        if self.kind == Kind.INT:
            return str(self.int_val)
        if self.kind == Kind.FLOAT:
            return repr(self.float_val)
        if self.kind == Kind.STRING:
            return '"' + self._escape_string(self.str_val) + '"'
        if self.kind == Kind.SYMBOL:
            return self._escape_symbol(self.str_val)
        if self.kind == Kind.KEYWORD:
            return ':' + self._escape_symbol(self.str_val)
        if self.kind == Kind.LIST:
            if not self.items:
                return '()'
            if pretty and len(self.items) > 1:
                parts = []
                for item in self.items:
                    parts.append(inner_indent + item.to_string(pretty, depth + 1))
                return '(\n' + '\n'.join(parts) + '\n' + indent + ')'
            else:
                parts = [item.to_string(pretty, depth) for item in self.items]
                return '(' + ' '.join(parts) + ')'
        return ''


def nil() -> Node:
    return Node(Kind.NIL)


def boolean(v: bool) -> Node:
    n = Node(Kind.BOOL)
    n.bool_val = v
    return n


def integer(v: int) -> Node:
    n = Node(Kind.INT)
    n.int_val = v
    return n


def real(v: float) -> Node:
    n = Node(Kind.FLOAT)
    n.float_val = v
    return n


def string(v: str) -> Node:
    return Node(Kind.STRING, v)


def symbol(v: str) -> Node:
    return Node(Kind.SYMBOL, v)


def keyword(v: str) -> Node:
    k = str(v)
    if k.startswith(':'):
        k = k[1:]
    return Node(Kind.KEYWORD, k)


def form(name: str) -> Node:
    k = str(name)
    if k.startswith(':'):
        k = k[1:]
    n = Node(Kind.LIST)
    n.items = [Node(Kind.KEYWORD, k)]
    return n


def list(nodes: _builtin_list[Node]) -> Node:
    n = Node(Kind.LIST)
    n.items = _builtin_list(nodes)
    return n
