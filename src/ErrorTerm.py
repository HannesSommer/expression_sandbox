#!/usr/bin/python
from sympy.matrices import *
from sympy.simplify.cse_main import cse
from sympy.printing import ccode
from sympy.core import Symbol
from sympy.utilities import numbered_symbols
from sympy import sin,cos, zeros, eye

def matrixify(a):
    try:
        if a.is_Matrix:
            return a
    except AttributeError:
        pass
    return Matrix([a])


class ErrorTerm:
    def __init__(self,parent=None,namespace=None,includes=[]):
        self.data = {}
        self.variables = []
        self.function = None
        self.includes = includes
        self.namespace = namespace
        self.parent = parent

    def declareData(self, name, dim=1):
        if dim == 1:
            v = Symbol(name)
        else:
            v = Matrix(dim,1,lambda i,j:Symbol("%s(%d)"%(name,i)))
        self.data[name] = v
        return v

    def declareVariable(self, name, dim=1):
        if dim == 1:
            v = Symbol(name)
        else:
            v = Matrix(dim,1,lambda i,j:Symbol("%s(%d)"%(name,i)))
        self.variables.append((name,v))
        return v

    def setFunction(self, f):
        self.function = matrixify(f)
        self.prepareJacobian()


    def prepareJacobian(self):
        l = [matrixify(x) for n,x in self.variables]
        if len(self.variables)==0:
            self.Vars = Matrix()
            self.J = zeros(self.function.rows,1)
        else:
            self.Vars = reduce(lambda a,b:a.col_join(b),l[1:],l[0])
            self.J = self.function.jacobian(self.Vars)

    def write_header(self,classname,filename=None,path=""):
        if not filename:
            filename = classname + ".h"
        f = open(path + "/" + filename,"w")
        f.write(self.generate_header(classname))
        f.close()

    def generate_header(self,classname):
        assert(self.function)
        ns_start = ""
        ns_end = ""
        if self.namespace:
            ns_start = "namespace %s {" % self.namespace
            ns_end = "}; // " + self.namespace
        includes = ""
        if self.includes:
            includes = "\n".join(["#include <%s>"%s for s in self.includes])
        inherit_from = ""
        if self.parent:
            inherit_from = ": " + self.parent
        return """
#ifndef __%s_H__
#define __%s_H__
#include <map>
#include <string>
#include <math.h>
#include <Eigen/Core>
// This file is auto-generated. Do not modify
%s

%s

class %s %s
{
    public:
        struct Variable {
            size_t index;
            size_t dim;
            Variable() {}
            Variable(size_t i, size_t d) : index(i), dim(d) {}
        };
        typedef std::map<std::string, Variable> VariableMap;
    protected:
        // The list of variables, their index and dimension
        VariableMap variables;
        // Function value and jacobian
        Eigen::VectorXd F;
        Eigen::MatrixXd J;
        // Data
%s
    public:
        // Constructor
%s

        const Eigen::VectorXd & getValue() const {return F;}
        const Eigen::MatrixXd & getJacobian() const {return J;}
        const VariableMap & getVariables() const {return variables;}

        // Evaluate J and F
%s
};

%s

#endif // __%s_H__
        """ % (classname.upper(),classname.upper(),includes,ns_start,classname,inherit_from,self.generateData(),\
                self.generateConstructor(classname),self.genEval(),ns_end,classname.upper())

    def generateData(self):
        text = ""
        for name,v in self.data.items():
            if v.is_Matrix:
                text += "        Eigen::MatrixXd %s;\n" % name
            else:
                text += "        double %s;\n" % name
        return text

    def generateConstructor(self,classname):
        text = "        %s(\n" % classname
        args = []
        for name,v in self.data.items():
            if v.is_Matrix:
                args.append("           const Eigen::MatrixXd & _%s_" % name)
            else:
                args.append("           double _%s_" % name)
        text += ",\n".join(args) + "\n"
        text += "        ) :\n"
        args =["           F(%d), J(%d,%d)" % (self.function.rows,self.J.rows,self.J.cols)]
        for name,v in self.data.items():
            if v.is_Matrix:
                args.append("           %s(_%s_)" % (name,name))
            else:
                args.append("           %s(_%s_)" % (name,name))
        text += ",\n".join(args) + "\n"
        text += "        {\n"
        idx = 0;
        for n,v in self.variables:
            dim = 1
            if v.is_Matrix:
                dim = v.rows
            text += "           variables[\"%s\"] = Variable(%d,%d);\n" \
                    % (n,idx,dim)
            idx += dim
        text += "        }\n"
        return text

    def genEval(self):
        text = "        bool evaluate(\n"
        args=[]
        for name,v in self.variables:
            if v.is_Matrix:
                args.append("           const Eigen::MatrixXd & %s" % name)
            else:
                args.append("           double %s" % name)
        args.append("           bool evalF=true,bool evalJ=true")
        text += ",\n".join(args) + ") {\n"
        text += "           if (evalF) {\n"
        (interm, expr) = cse(self.function,numbered_symbols("__x"));
        for dummy,exp in interm:
            text += "               double %s = %s;\n" % (str(dummy),ccode(exp))
        for i in range(self.function.rows):
            text += "               F(%d) = %s;\n" % (i,ccode(expr[0][i]))
        text += "           }\n"
        text += "           if (evalJ) {\n"
        (interm, expr) = cse(self.J,numbered_symbols("__x"));
        for dummy,exp in interm:
            text += "               double %s = %s;\n" % (str(dummy),ccode(exp))
        for i in range(self.J.rows):
            for j in range(self.J.cols):
                text += "               J(%d,%d) = %s;\n" % (i,j,ccode(expr[0][i,j]))
        text += "           }\n"
        text += "           return true;\n"
        text += "       }\n"
        return text


