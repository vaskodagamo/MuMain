"""Monotone cubic interpolation preserves authored tips and directional extrema."""
from mathutils import Vector


def slopes(times, values):
    intervals = [b-a for a,b in zip(times,times[1:])]
    secants = [(b-a)/h for a,b,h in zip(values,values[1:],intervals)]
    result = [secants[0]]
    for index in range(1,len(values)-1):
        left,right = secants[index-1:index+1]
        if left*right <= 0:
            result.append(0)
            continue
        h0,h1 = intervals[index-1:index+1]
        w0,w1 = 2*h1+h0,h1+2*h0
        result.append((w0+w1)/(w0/left+w1/right))
    result.append(secants[-1])
    return result


def evaluate(times, points, value):
    index = next((i for i,t in enumerate(times[1:]) if value <= t),len(times)-2)
    span = times[index+1]-times[index]
    t = (value-times[index])/span
    weights = (2*t**3-3*t**2+1,t**3-2*t**2+t,-2*t**3+3*t**2,t**3-t**2)
    result = []
    for axis in range(3):
        values = [p[axis] for p in points]
        derivatives = slopes(times,values)
        samples = (values[index],span*derivatives[index],values[index+1],span*derivatives[index+1])
        result.append(sum(a*b for a,b in zip(weights,samples)))
    return Vector(result)


def long_arc(times, points, value):
    """Retain endpoints/extrema; redistribute obsolete monotonic interior controls."""
    result=[]
    for axis in range(3):
        keep=[0]+[i for i in range(1,len(times)-1)
            if (points[i][axis]-points[i-1][axis])*(points[i+1][axis]-points[i][axis])<=0]+[len(times)-1]
        selected_times=[times[i] for i in keep]
        selected=[Vector((points[i][axis],)*3) for i in keep]
        result.append(evaluate(selected_times,selected,value)[0])
    return Vector(result)
