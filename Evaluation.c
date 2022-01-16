#include "Shell.h"
#include "Evaluation.h"

#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/*typedef enum expr_t
{
  VIDE,           // Commande vide
  SIMPLE,         // Commande simple
  SEQUENCE,       // S�quence (;)
  SEQUENCE_ET,    // S�quence conditionnelle (&&)
  SEQUENCE_OU,    // S�quence conditionnelle (||)
  BG,             // Tache en arriere plan
  PIPE,           // Pipe
  REDIRECTION_I,  // Redirection entree
  REDIRECTION_O,  // Redirection sortie standard
  REDIRECTION_A,  // Redirection sortie standard, mode append
  REDIRECTION_E,  // Redirection sortie erreur
  REDIRECTION_EO, // Redirection sorties erreur et standard
} expr_t; */

/*typedef struct Expression
{
  expr_t type;
  struct Expression *gauche;
  struct Expression *droite;
  char **arguments;
} Expression;*/

int simple_command(char** args){

  pid_t pid=fork();
  if (pid==0){
    int err=execvp(args[0], args);
    exit(err);
  }
  else{
    int status;
    wait(&status);
    return status;
  }
}


int redir_in(char* filename, Expression *e){
  int old_stdin=dup(STDIN_FILENO);
  int fd=open(filename, O_RDONLY);
  dup2(fd,STDIN_FILENO);
  close(fd);
  int status=evaluer_expr(e->gauche);
  dup2(old_stdin,STDIN_FILENO);
  close(old_stdin);
  return status;
}

int redir_out(char* filename, Expression *e){
  int old_stdout=dup(STDOUT_FILENO);

  if (e->type == REDIRECTION_O){
    int fd=open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    dup2(fd,STDOUT_FILENO);
    close(fd);
    int status=evaluer_expr(e->gauche);
    dup2(old_stdout,STDOUT_FILENO);
    close(old_stdout);
    return status;
  }
  if (e->type == REDIRECTION_A){
    int fd=open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0666);
    dup2(fd,STDOUT_FILENO);
    close(fd);
    int status=evaluer_expr(e->gauche);
    dup2(old_stdout,STDOUT_FILENO);
    close(old_stdout);
    return status;
  }
}

int redir_err(char* filename, Expression *e){
  int old_stderr=dup(STDERR_FILENO);

  if (e->type == REDIRECTION_EO){
    int old_stdout=dup(STDOUT_FILENO);
    int fd=open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);

    dup2(fd,STDERR_FILENO);
    dup2(fd,STDOUT_FILENO);
    close(fd);
    int status=evaluer_expr(e->gauche);
    dup2(old_stderr,STDERR_FILENO);
    dup2(old_stdout,STDOUT_FILENO);
    close(old_stderr);
    close(old_stdout);
    return status;
  }

  if (e->type == REDIRECTION_E){
    int fd=open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    dup2(fd,STDERR_FILENO);
    close(fd);
    int status=evaluer_expr(e->gauche);
    dup2(old_stderr,STDERR_FILENO);
    close(old_stderr);
    return status;
  }

}

int evaluer_expr(Expression *e)
{
  int status;
  int child_zom=waitpid(-1,&status,WNOHANG);
  if(child_zom>0){
    printf("Child %d exited with code: %d\n",child_zom,status);
  }

  

  if (e->type == VIDE)
    return 0;
  
  if (e->type == SIMPLE){
    if (!strcmp(e->arguments[0],"echo")){
      int i=1;
      int n,l;
      while(e->arguments[i]){
        l=strlen(e->arguments[i]);
        n=write(STDOUT_FILENO,e->arguments[i],l);
        if (n!=l){
          return 1;
        }
        n=write(STDOUT_FILENO," ",sizeof(char));
        if (n!=1){
          return 1;
        }
        i++;
      }
      n=write(STDOUT_FILENO,"\n",sizeof(char));
      if (n!=1){
        return 1;
      }
      return 0;
    }

    else{
      return simple_command(e->arguments);
    }
  }

  if (e->type == SEQUENCE){
    evaluer_expr(e->gauche);
    return  evaluer_expr(e->droite);
  }

  if (e->type == SEQUENCE_ET){
    int err=evaluer_expr(e->gauche);
    if (err==0){
      return evaluer_expr(e->droite);
    }
    return err;
  }

  if (e->type == SEQUENCE_OU){
    if (evaluer_expr(e->gauche)!=0){
      return evaluer_expr(e->droite);
    }
    return 0;
  }

  if (e->type == BG){
    int status;
    if (!fork()){
      evaluer_expr(e->gauche);
      exit(status);
    }
    return 0;
  }

  if (e->type == REDIRECTION_I){
    return redir_in(e->arguments[0],e);
  }

  if (e->type == REDIRECTION_O || e->type == REDIRECTION_A){
    return redir_out(e->arguments[0],e);
  }

  if (e->type == REDIRECTION_E || e->type == REDIRECTION_EO){
    return redir_err(e->arguments[0],e);
  }
  
  if(e->type ==PIPE){
    int tube[2];
    if(pipe(tube)==-1){
      return 1;
    }
    
    if (fork()==0){
      close(tube[0]);
      int old_stdout=dup(STDOUT_FILENO);
      dup2(tube[1],STDOUT_FILENO);
      close(tube[1]);
      exit(evaluer_expr(e->gauche));
    }

    else{
      close(tube[1]);
      int old_stdin=dup(STDIN_FILENO);
      dup2(tube[0],STDIN_FILENO);
      close(tube[0]);

      int return_value=evaluer_expr(e->droite);
      dup2(old_stdin,STDIN_FILENO);
      wait(&status);
      return return_value;
    }


  }

  fprintf(stderr, "not yet implemented \n");
  return 1;
}
