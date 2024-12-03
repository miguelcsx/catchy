def a_decorator(a, b): # 0
    def inner(func): # 0
        return func # 0
    return inner # 0

def b_decorator(a, b): # 0
    def inner(func): # 0
        if func: # 1 (nested = 0), total 1
            return None # 0
        return func # 0
    return inner # 0
