from heapq import merge

def merge_sort2(m):
    """Sort list, using two part merge sort"""
    if len(m) <= 1:
        return m

    middle = len(m) // 2
    left = merge_sort2(m[:middle])
    right = merge_sort2(m[middle:])
    return list(merge(left, right))

def merge_sort4(m):
    """Sort list, using four part merge sort"""
    if len(m) <= 4:
        return sorted(m)

    middle = len(m) // 2
    left_middle = middle // 2
    right_middle = middle + left_middle

    parts = [
        m[:left_middle],
        m[left_middle:middle],
        m[middle:right_middle],
        m[right_middle:]
    ]

    sorted_parts = [merge_sort4(p) for p in parts]
    merged1 = list(merge(sorted_parts[0], sorted_parts[1]))
    merged2 = list(merge(sorted_parts[2], sorted_parts[3]))
    return list(merge(merged1, merged2))